
#include <synapse/Synapse>
#include <synapse/SynapseMain.hpp>

#include "quadtree.h"
#include "bh_renderer.h"


using namespace Syn;

//
class layer : public Layer
{
public:
    layer() : Layer("layer") {}
    virtual ~layer() {}

    virtual void onAttach() override;
    virtual void onUpdate(float _dt) override;
    virtual void onImGuiRender() override;
    void onKeyDownEvent(Event *_e);
    void onMouseButtonEvent(Event *_e);

    // debug functions
    void __debug_setup_rnorm();
    void __debug_setup_empty();
    void __debug_setup_BH_test();
    //
    void __debug_update_tf_point();
    //
    void __debug_tree_interaction();
    void __debug_insert_on_rclick();


public:
    Ref<Framebuffer> m_renderBuffer = nullptr;
    Ref<Font> m_font = nullptr;
    // flags
    bool m_wireframeMode = false;
    bool m_toggleCulling = false;

    //
    Ref<QuadtreeBH> m_qt;
    Ref<BHRenderer> m_renderer;
    Ref<OrthographicCamera> m_camera;

    // DEBUG : input
    glm::vec4 m_tf_point;
    QuadtreeBH *m_selQT = NULL;
    size_t m_selQT_pointCount = 0;
    uint32_t m_selQT_level = 0;
    glm::vec2 m_sel_vertex = glm::vec2(0.0f);
    
    //
    const size_t N = 1000000;
    std::vector<glm::vec2> m_points;

    // flags
    bool m_cameraMode = false;
    bool m_renderAABB = true;
};

//
class syn_app_instance : public Application
{
public:
    syn_app_instance() { this->pushLayer(new layer); }
};
Application* CreateSynapseApplication() { return new syn_app_instance(); }

//----------------------------------------------------------------------------------------
void layer::__debug_setup_rnorm()
{
    // test data
    std::random_device rd{};
    std::mt19937 gen{rd()};
    std::normal_distribution<float> norm{0.0f, 25.0f};
    glm::vec2 min(1000.0f), max(-1000.0f);
    for (int i = 0; i < N; i++)
    {
        glm::vec2 p = { norm(gen), norm(gen) };
        m_points.push_back(p);
        min.x = std::min(p.x, min.x);
        min.y = std::min(p.y, min.y);
        max.x = std::max(p.x, max.x);
        max.y = std::max(p.y, max.y);
    }
    //
    m_qt = std::make_shared<QuadtreeBH>(N);

    // insert points into tree (scaled to [-1 .. 1])
    glm::vec2 inv_range = 1.0f / (max - min);
    Timer t;
    for (auto &p : m_points)
    {
        p = (((p - min) * inv_range) - 0.5f) * 2.0f;
        m_qt->insert(m_qt, glm::vec2(p.x, p.y));
    }

    SYN_TRACE("tree created in ", t.getDeltaTimeMs(), "ms.");

}

//----------------------------------------------------------------------------------------
void layer::__debug_setup_empty()
{
    m_qt = std::make_shared<QuadtreeBH>(N);

}

//----------------------------------------------------------------------------------------
void layer::__debug_setup_BH_test()
{
    std::random_device rd{};
    std::mt19937 gen{rd()};
    std::normal_distribution<float> norm{ 0.0f, 0.05f };
    std::uniform_real_distribution<float> uniform{ -0.9f, 0.9f };
    Timer t;

    m_qt = std::make_shared<QuadtreeBH>(N);
    int n_groupings = 300;
    int n_per_group = 300;
    for (int i = 0; i < n_groupings; i++)
    {
        glm::vec2 mpos = glm::vec2(uniform(gen), uniform(gen));
        for (int i = 0; i < n_per_group; i++)
        {
            glm::vec2 p = glm::vec2(norm(gen), norm(gen)) + mpos;
            p.x = clamp(p.x, -1.0f, 1.0f);
            p.y = clamp(p.y, -1.0f, 1.0f);
            m_qt->insert(m_qt, p);
        }
        
    }
    SYN_TRACE("tree created in ", t.getDeltaTimeMs(), "ms.");

}

//----------------------------------------------------------------------------------------
void layer::onAttach()
{
    // register event callbacks
    EventHandler::register_callback(EventType::INPUT_KEY, SYN_EVENT_MEMBER_FNC(layer::onKeyDownEvent));
    EventHandler::register_callback(EventType::INPUT_MOUSE_BUTTON, SYN_EVENT_MEMBER_FNC(layer::onMouseButtonEvent));
    EventHandler::push_event(new WindowToggleFullscreenEvent());

    m_renderBuffer = API::newFramebuffer(ColorFormat::RGBA16F, glm::ivec2(0), 1, true, true, "render_buffer");

    // load font
    m_font = MakeRef<Font>("../assets/ttf/JetBrains/JetBrainsMono-Medium.ttf", 14.0f);
    m_font->setColor(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

    // QuadtreeBH tests -----------------------------------------------------------------

    // generate tree
    // __debug_setup_rnorm();
    // __debug_setup_empty();
    __debug_setup_BH_test();

    // Initialize QuadtreeBH renderer (BHRenderer)
    m_renderer = std::make_shared<BHRenderer>(m_qt);
    
    // Initialize camera
    m_camera = API::newOrthographicCamera(1.0f, 50.0f);
    m_camera->setMoveSpeed(2.0f);
    m_camera->setZoomLevel(1.0f);
    m_camera->setZoomSpeed(0.05f);
    m_camera->setZoomLimit(0.001f);
    m_camera->setZoomAmplifier(1.02f);

    // general settings
	Renderer::get().setClearColor(0.2f, 0.2f, 0.2f, 1.0f);
	Renderer::get().disableImGuiUpdateReport();
    // Application::get().setMaxFPS(30.0f);
}

//----------------------------------------------------------------------------------------
void layer::__debug_update_tf_point()
{
    static auto &renderer = Renderer::get();
    auto &mpos = InputManager::get_mouse_position();
    auto &offset2 = renderer.getImGuiDockingPositionF();
    auto &vp = renderer.getViewportF();
    glm::vec2 px = { mpos.x, mpos.y - offset2.y };

    // transform to [-1 .. 1]
    px = (px / vp) * 2.0f - 1.0f;
    px.y = -px.y;

    // transform to local AABB coordinates
    m_tf_point = glm::vec4(px.x, px.y, 0.0f, 1.0f) * glm::inverse(m_camera->getViewProjectionMatrix());
    m_tf_point += glm::vec4(m_camera->getPosition().x, m_camera->getPosition().y, 0.0f, 0.0f);

}

//----------------------------------------------------------------------------------------
void layer::__debug_tree_interaction()
{    
    // select subtree
    m_qt->getSelectedSubtree(m_qt, m_tf_point, &m_selQT);

    if (m_selQT != NULL)
    {
        m_selQT_pointCount = m_selQT->getLocalVertices().size();
        m_selQT_level = m_selQT->getLevel();
        // highlight selected AABB
        m_renderer->highlightAABB(m_selQT->getAABB());
        // highlight closest vertex
        m_selQT->getClosestVertex(m_selQT, m_tf_point, m_sel_vertex);
        m_renderer->highlightVertex(m_sel_vertex);
    }

    // Enabling Barnes-Hut approximation
    if (m_renderer->getRenderBH())
    {
        // use highlighted vertex as query if enabled, else mouse pos
        glm::vec2 cmp_vertex = { m_tf_point.x, m_tf_point.y };
        if (m_renderer->getRenderHighlightVertex() && m_sel_vertex != glm::vec2(0.0f))
            cmp_vertex = m_sel_vertex;

        std::vector<glm::vec3> v_BH;
        m_qt->approxBH(m_qt, cmp_vertex, v_BH);
        m_renderer->highlightBH(v_BH);
    }

}

//----------------------------------------------------------------------------------------
void layer::__debug_insert_on_rclick()
{
    glm::vec2 mpos = { m_tf_point.x, m_tf_point.y };

    std::random_device rd{};
    std::mt19937 gen{rd()};
    std::normal_distribution<float> norm{ 0.0f, 0.025f };

    for (int i = 0; i < 10; i++)
    {
        glm::vec2 p = glm::vec2(norm(gen), norm(gen)) + mpos;
        p.x = clamp(p.x, -1.0f, 1.0f);
        p.y = clamp(p.y, -1.0f, 1.0f);
        m_qt->insert(m_qt, p);
    }

    m_renderer->updateGeometry();

}

//----------------------------------------------------------------------------------------
void layer::onUpdate(float _dt)
{
    SYN_PROFILE_FUNCTION();
	
    static auto& renderer = Renderer::get();
    
    m_renderBuffer->bind();
    if (m_wireframeMode)
        renderer.enableWireFrame();    
    renderer.clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


    // -- BEGINNING OF SCENE -- //

    // update mouse position relative to camera and tree
    __debug_update_tf_point();

    // interact with tree
    __debug_tree_interaction();

    m_camera->onUpdate(_dt);
    m_renderer->render(m_camera);

    // -- END OF SCENE -- //


    if (m_wireframeMode)
        renderer.disableWireFrame();

	
    // Text rendering 
    // TODO: all text rendering should go into an overlay layer.
    static float fontHeight = m_font->getFontHeight() + 1.0f;
    int i = 0;
    static int depth = m_qt->depth(m_qt);
    //
    m_font->beginRenderBlock();
	m_font->addString(2.0f, fontHeight * ++i, "fps=%.0f  VSYNC=%s", TimeStep::getFPS(), Application::get().getWindow().isVSYNCenabled() ? "ON" : "OFF");
    m_font->addString(2.0f, fontHeight * ++i, "CAMERA zoom level: %.4f", m_camera->getZoomLevel());
    m_font->addString(2.0f, fontHeight * ++i, "RENDER:  AABB[F1] %s  BH[TAB] %s  hl AABB[F2] %s  hl vertex[F3] %s",
        m_renderer->getRenderAABB() ? "true " : "false",
        m_renderer->getRenderBH() ? "true " : "false",
        m_renderer->getRenderHighlightAABB() ? "true " : "false",
        m_renderer->getRenderHighlightVertex() ? "true " : "false");
    m_font->addString(2.0f, fontHeight * ++i, "sel vcount = %zu, sel level = %d", m_selQT_pointCount, m_selQT_level);
    size_t vcount = m_renderer->getTotalVertexCount();
    size_t bh_vcount = m_renderer->getBHVertexCount();
    m_font->addString(2.0f, fontHeight * ++i, "total vertices = %zu", vcount);
    m_font->addString(2.0f, fontHeight * ++i, "BH vertices    = %zu (%.2f%%)", bh_vcount, 100.0f * (float)bh_vcount / (float)vcount);
    m_font->addString(2.0f, fontHeight * ++i, "theta = %.2f", s_thetaBH);
    m_font->endRenderBlock();

    //
    m_renderBuffer->bindDefaultFramebuffer();
}
 
//----------------------------------------------------------------------------------------
void layer::onKeyDownEvent(Event *_e)
{
    KeyDownEvent *e = dynamic_cast<KeyDownEvent*>(_e);
    static bool vsync = true;

    if (e->getAction() == SYN_KEY_PRESSED)
    {
        switch (e->getKey())
        {
            case SYN_KEY_Z:
                vsync = !vsync;
                Application::get().getWindow().setVSYNC(vsync);
                break;
            case SYN_KEY_V:         m_renderBuffer->saveAsPNG();            break;
            case SYN_KEY_ESCAPE:    EventHandler::push_event(new WindowCloseEvent());
                break;
            case SYN_KEY_PLUS:      s_thetaBH = clamp(s_thetaBH + 0.05f, 0.0f, 1.0f);   break;
            case SYN_KEY_MINUS:     s_thetaBH = clamp(s_thetaBH - 0.05f, 0.0f, 1.0f);   break;
            case SYN_KEY_TAB:       m_renderer->toggleRenderBH();           break;
            case SYN_KEY_F1:        m_renderer->toggleAABB();               break;
            case SYN_KEY_F2:        m_renderer->toggleHighlightAABB();      break;
            case SYN_KEY_F3:        m_renderer->toggleHighlightVertex();    break;
            case SYN_KEY_F4:        m_wireframeMode = !m_wireframeMode;     break;
            case SYN_KEY_F5:    
                m_toggleCulling = !m_toggleCulling;
                Renderer::setCulling(m_toggleCulling);
                break;
            default: break;
        }
    }
}

//----------------------------------------------------------------------------------------
void layer::onMouseButtonEvent(Event *_e)
{
    MouseButtonEvent *e = dynamic_cast<MouseButtonEvent *>(_e);

    switch (e->getButton())
    {
        case SYN_MOUSE_BUTTON_1:
            break;
        
        case SYN_MOUSE_BUTTON_2:
            if (e->getAction() == SYN_MOUSE_PRESSED)
                __debug_insert_on_rclick();
            break;
        
        default:
            break;
    }

}

//----------------------------------------------------------------------------------------
void layer::onImGuiRender()
{
    static bool p_open = true;

    static bool opt_fullscreen_persistant = true;
    static ImGuiDockNodeFlags opt_flags = ImGuiDockNodeFlags_None;
    bool opt_fullscreen = opt_fullscreen_persistant;

    // We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
    // because it would be confusing to have two docking targets within each others.
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    if (opt_fullscreen)
    {
    	ImGuiViewport* viewport = ImGui::GetMainViewport();
    	ImGui::SetNextWindowPos(viewport->Pos);
    	ImGui::SetNextWindowSize(viewport->Size);
    	ImGui::SetNextWindowViewport(viewport->ID);
    	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    	window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    	window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    }

    // When using ImGuiDockNodeFlags_PassthruDockspace, DockSpace() will render our background and handle the pass-thru hole, so we ask Begin() to not render a background.
    if (opt_flags & ImGuiDockNodeFlags_PassthruCentralNode)
	    window_flags |= ImGuiWindowFlags_NoBackground;

    window_flags |= ImGuiWindowFlags_NoTitleBar;

    ImGui::GetCurrentContext()->NavWindowingToggleLayer = false;

    //-----------------------------------------------------------------------------------
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("synapse-core", &p_open, window_flags);
    ImGui::PopStyleVar();

    if (opt_fullscreen)
	    ImGui::PopStyleVar(2);

    // Dockspace
    ImGuiIO& io = ImGui::GetIO();
    ImGuiID dockspace_id = 0;
    if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
    {
        dockspace_id = ImGui::GetID("dockspace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), opt_flags);
    }
	
    //-----------------------------------------------------------------------------------
    // set the 'rest' of the window as the viewport
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("synapse-core::renderer");
    static ImVec2 oldSize = { 0, 0 };
    ImVec2 viewportSize = ImGui::GetContentRegionAvail();

    if (viewportSize.x != oldSize.x || viewportSize.y != oldSize.y)
    {
        // dispatch a viewport resize event -- registered classes will receive this.
        EventHandler::push_event(new ViewportResizeEvent(glm::vec2(viewportSize.x, viewportSize.y)));
        SYN_CORE_TRACE("viewport [ ", viewportSize.x, ", ", viewportSize.y, " ]");
        oldSize = viewportSize;
    }

    // direct ImGui to the framebuffer texture
    ImGui::Image((void*)m_renderBuffer->getColorAttachmentIDn(0), viewportSize, { 0, 1 }, { 1, 0 });

    ImGui::End();
    ImGui::PopStyleVar();


    // end root
    ImGui::End();

}
