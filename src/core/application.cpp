#include "application.h"

#include "rendering/renderer.h"
#include "settings_manager.h"
#include "hdri_manager.h"

#include "states/simulation_state.h"
#include "states/config_state.h"
#include "states/world_builder_state.h"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <ImGuizmo.h>

#include "platform/vulkan/vulkan_renderer.h"

#ifdef __APPLE__
#include "platform/metal/metal_context.h"
#include "platform/vulkan/vulkan_context.h"
#endif

namespace Donut
{
    Application* Application::s_instance = nullptr;

    Application::Application(const std::string& name, int width, int height)
        : m_running(true), m_minimized(false)
    {
        s_instance = this;

        // Select the render API before the window is created: the window is
        // built differently for Vulkan (GLFW_NO_API) than for OpenGL.
        Logger::init();
        SettingsManager::initialize();
        {
            const auto& settings = SettingsManager::get_settings_const();
            RendererAPI::set_api(settings.graphics.render_api == "Vulkan"
                                ? RendererAPI::API::Vulkan : RendererAPI::API::OpenGL);
        }

        if (RendererAPI::get_api() == RendererAPI::API::Vulkan)
            vulkan_prepare_glfw(); // must precede glfwInit() inside the Window ctor

        m_window = create_scope<Window>(name, width, height);
        m_window->set_event_callback([this](Event& event)
        {
            on_event(event);
        });

        on_init();
    }

    Application::~Application()
    {
        on_shutdown();
        s_instance = nullptr;
    }

    auto Application::run() -> void
    {
        while (m_running)
        {
            if (m_vulkan_renderer)
            {
                glfwPollEvents(); // before the ImGui frame so input is current
                if (!m_minimized)
                    m_vulkan_renderer->draw_frame(glm::vec4(0.05f, 0.06f, 0.10f, 1.0f),
                                                [this] { build_vulkan_ui(); });
            }
            else
            {
                if (!m_minimized)
                {
                    on_update();
                    on_render();
                }
                m_window->on_update(); // polls events + swaps buffers (OpenGL)
            }
        }
    }

    auto Application::build_vulkan_ui() -> void
    {
        ImGui::Begin("Donut - Vulkan backend");
        ImGui::Text("Geodesic black hole through Vulkan (MoltenVK).");
        ImGui::Text("%.1f FPS", ImGui::GetIO().Framerate);
        ImGui::Separator();

        if (m_vulkan_renderer)
        {
            bool scene = m_vulkan_renderer->is_scene_mode();
            ImGui::Text("View");
            if (ImGui::RadioButton("Black hole", !scene)) m_vulkan_renderer->set_scene_mode(false);
            ImGui::SameLine();
            if (ImGui::RadioButton("Scene", scene))       m_vulkan_renderer->set_scene_mode(true);
            ImGui::Separator();

            if (scene)
            {
                ImGui::TextDisabled("World-builder scene (grid). Drag to orbit, scroll to zoom.");
                ImGui::TextDisabled("Lit sphere + skybox coming next.");
            }
            else
            {
                bool free_fly = m_vulkan_renderer->is_free_fly();
                ImGui::Text("Camera");
                if (ImGui::RadioButton("Orbital", !free_fly))  m_vulkan_renderer->set_free_fly(false);
                ImGui::SameLine();
                if (ImGui::RadioButton("Free-fly", free_fly))  m_vulkan_renderer->set_free_fly(true);
                ImGui::TextDisabled(free_fly ? "WASD move | Q/E down-up | Shift boost | drag to look"
                                             : "Drag to orbit | scroll to zoom");
                ImGui::Separator();

                auto& hdri = HDRIManager::get();
                std::string current = m_vulkan_renderer->current_hdri();
                std::string preview = hdri.get_hdri_name(current);
                if (ImGui::BeginCombo("HDRI", preview.c_str()))
                {
                    for (const auto& path : hdri.get_available_hdri())
                    {
                        bool selected = (path == current);
                        if (ImGui::Selectable(hdri.get_hdri_name(path).c_str(), selected))
                            m_vulkan_renderer->set_hdri(path);
                        if (selected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                ImGui::TextDisabled("Switching rebuilds the cubemap (brief pause).");
            }
        }

        ImGui::End();
    }

    auto Application::close() -> void
    {
        m_running = false;
    }

    auto Application::on_event(Event& event) -> void
    {
        EventDispatcher dispatcher(event);
        dispatcher.dispatch<WindowCloseEvent>([this, &event](WindowCloseEvent& e)
        {
            m_running = false;
            event.handled = true;
            return true;
        });

        dispatcher.dispatch<WindowResizeEvent>([this, &event](WindowResizeEvent& e)
        {
            if (e.get_width() == 0 || e.get_height() == 0)
                m_minimized = true;
            else
                m_minimized = false;

            if (m_vulkan_renderer)
            {
                m_vulkan_renderer->on_resize(e.get_width(), e.get_height());
            }
            else
            {
                Renderer::on_window_resize(e.get_width(), e.get_height());
                if (m_engine) m_engine->set_window_dimensions(e.get_width(), e.get_height());
            }
            event.handled = true;
            return true;
        });

        dispatcher.dispatch<KeyPressedEvent>([this, &event](KeyPressedEvent& e)
        {
            if (!m_state_manager)
                return true;
            if (e.get_key_code() == GLFW_KEY_1)
            {
                m_state_manager->switch_to_state("Config");
                event.handled = true;
                return true;
            }
            else if (e.get_key_code() == GLFW_KEY_2)
            {
                m_state_manager->switch_to_state("Simulation");
                event.handled = true;
                return true;
            }
            else if (e.get_key_code() == GLFW_KEY_3)
            {
                m_state_manager->switch_to_state("WorldBuilder");
                event.handled = true;
                return true;
            }

            return true;
        });

        if (m_state_manager)
            m_state_manager->on_event(event);
    }

    auto Application::on_init() -> void
    {
        // (Logger, settings and API selection happen in the constructor.)

        if (RendererAPI::get_api() == RendererAPI::API::Vulkan)
        {
            int w = 0, h = 0;
            glfwGetFramebufferSize((GLFWwindow*)m_window->get_native_window(), &w, &h);
            m_vulkan_renderer = create_scope<VulkanRenderer>();
            if (!m_vulkan_renderer->init(m_window->get_native_window(), w, h))
                DONUT_ERROR("Vulkan renderer initialization failed");
            else
                m_vulkan_renderer->init_im_gui();
            // Scene rendering hooks in here in the B-3 phase; the Vulkan path now
            // clears, presents, and draws the ImGui UI.
            return;
        }

        Renderer::init();
        m_window->init_im_gui();

        Renderer::on_window_resize(1280, 720);
        RenderCommand::set_face_culling(false);

        m_engine = create_scope<Engine>();
        m_engine->set_window_dimensions(1280, 720);

        m_state_manager = create_scope<StateManager>();
        m_state_manager->register_state("Config",       create_scope<ConfigState>());
        m_state_manager->register_state("Simulation",   create_scope<SimulationState>());
        m_state_manager->register_state("WorldBuilder", create_scope<WorldBuilderState>());
        m_state_manager->switch_to_state("Config");
    }

    auto Application::on_shutdown() -> void
    {
        if (m_vulkan_renderer)
        {
            m_vulkan_renderer->shutdown();
            m_vulkan_renderer.reset();
        }
        else
        {
            if (m_state_manager)
                m_state_manager->shutdown();
            Renderer::shutdown();
        }
        SettingsManager::shutdown();
        Logger::shutdown();
    }

    auto Application::on_update() -> void
    {
        float current_frame = (float)glfwGetTime();
        m_delta_time = current_frame - m_last_frame;
        m_last_frame = current_frame;

        m_state_manager->update(m_delta_time);
    }

    auto Application::on_render() -> void
    {
        m_state_manager->render();
        m_window->begin_im_gui_frame();

        ImGuizmo::BeginFrame();
        ImGuizmo::SetOrthographic(false);

        setup_docking_layout();
        m_state_manager->on_im_ui_render();
        m_window->end_im_gui_frame();
    }

    auto Application::setup_docking_layout() -> void
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::DockSpaceOverViewport(0, viewport, ImGuiDockNodeFlags_PassthruCentralNode);
    }
};
