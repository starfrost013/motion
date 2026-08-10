/* 
    m  o  t  i  o  n
    The SGI Emulator

    Copyright (c)2026 starfrost

    render.hpp: Backend independent render stuff

    TODO: Fully genericise this to be backend independent by eg adding a platform independent window class.
*/

#pragma once
#include <Motion.hpp>

namespace Motion
{
    #define RENDER_LOG_PREFIX                   "Render - Core"
    #define DEFAULT_TEXTURE_BYTES_PER_PIXEL     4                   // Not sure why we would need anything else...

    // forward declare
    class Renderer;
    
    // a colour in rgba format like sgi uses
    // use imvec4 if ui code
    struct Color
    {
        uint8_t r;
        uint8_t g;
        uint8_t b;
        uint8_t a; 

        Color(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
        {
            this->r = r;
            this->g = g;
            this->b = b;
            this->a = a; 
        }

        Color() : Color(0, 0, 0, 0) { };
    }; 

    /// @brief this is the renderer independent window class
    class Window
    {  
        // THis is the iris 3130's screen resolution. DON'T HARDCODE IT! MAKE IT A CVAR LATER
        #define WINDOW_DEFAULT_SIZE_X       1024
        #define WINDOW_DEFAULT_SIZE_Y       768

    public:
        virtual void Start() { };
        int32_t GetWindowSizeX() { return sizeX; }
        int32_t GetWindowSizeY() { return sizeY; }

        /// @brief sets the window size 
        /// @param x the x coordinate of the size to set
        /// @param y the y coordinate of the size to set
        virtual void SetWindowSize(int32_t x, int32_t y) { };

        virtual void Shutdown() { }; 

    protected:
        int32_t sizeX;
        int32_t sizeY;
    }; 

    // The render texture draw type of the screen.
    enum RenderTextureDrawType
    {
        Default = 0,            // Normal render. Source and destination are the same.
        Scaled = 1,             // Scale the texture while rendering and optionally only render part of it. This one uses the srcSize and destSize fields.
       
        // Always draw this texture the same size of the window and only sample from the portion of this texture within the window.
        // This is basically just a special case for the IRIS GPU.
        DrawAsWindowSize = 2,   
    }; 

    // base class for render texture. this is so we could use something other than SDL in the future.
    class RenderTexture
    {
    public: 
        /// @brief Constructor for a render texture with separate source and destination sizes.
        /// @param renderer The renderer to use for this texture.
        /// @param sizeX The source size of this texture, X coordinate.
        /// @param sizeY The source size of this texture, Y coordinate.
        /// @param drawType the draw type to use for this texture.
        RenderTexture(Renderer* renderer, int32_t sizeX, int32_t sizeY, RenderTextureDrawType drawType = RenderTextureDrawType::Default) 
        { 
            this->renderer = renderer; 
            this->sizeX = sizeX;
            this->sizeY = sizeY;
            // sensible defaults just i case
            this->destSizeX = this->srcSizeX = sizeX;
            this->destSizeY = this->srcSizeY = sizeY;
            this->drawType = drawType;
            this->stride = sizeX * DEFAULT_TEXTURE_BYTES_PER_PIXEL;
            this->pixels = new uint8_t[GetMemorySize()];
        };

        /// @brief Constructor for a render texture with separate source and destination sizes.
        /// @param renderer The renderer to use for this texture.
        /// @param sizeX The source size of this texture, X coordinate.
        /// @param sizeY The source size of this texture, Y coordinate.
        /// @param srcSizeX The area of this texture to render, X coordinate.
        /// @param srcSizeY The area of this texture to render, Y coordinate.
        /// @param destSizeX The final scaled size to render this texture (see RenderTextureDrawType), X coordinate.
        /// @param destSizeY The final scaled size to render this texture (see RenderTextureDrawType), Y coordinate. 
        /// @param drawType the draw type to use for this texture.
        RenderTexture(Renderer* renderer, int32_t sizeX, int32_t sizeY, 
            int32_t srcSizeX, int32_t srcSizeY, int32_t destSizeX, int32_t destSizeY, RenderTextureDrawType drawType = RenderTextureDrawType::Default)
        : RenderTexture(renderer, sizeX, sizeY, drawType)
        {
            this->srcSizeX = srcSizeX;
            this->srcSizeY = srcSizeY;
            this->destSizeX = destSizeX;
            this->destSizeY = destSizeY;
        }
        
        uint32_t sizeX = 0, sizeY = 0, srcSizeX = 0, srcSizeY = 0, destSizeX = 0, destSizeY = 0, stride = 0; 

        uint32_t GetPixel(int32_t x, int32_t y, Color color);
        Color GetPixel(int32_t x, int32_t y);
        void SetPixel(int32_t x, int32_t y, Color color); 
        void SetPixel(int32_t x, int32_t y, uint32_t color);

        // getters for private fields
        uint8_t* GetPixels() { return pixels; };
        uint64_t GetMemorySize() { return (sizeX * sizeY) << 2; };

        RenderTextureDrawType drawType = RenderTextureDrawType::Default;

        virtual ~RenderTexture() { };
    protected:
        Renderer* renderer; 
        uint8_t* pixels;
        // this is a mirror texture that gets uploaded to the
    }; 

    /// @brief this class defines a render pass so that we can do e.g. GF2->UC4->DC4
    class RenderPass
    {
    public: 
        virtual void Render(Renderer* renderer, RenderTexture* screen) { };
        const char* GetName() { return name; };

        // constructors

        RenderPass(const char* name)
        {
            strncpy(this->name, name, STRING_MAX_SHORT);
        }

        // have a parameterless constructor for e.g. standard arrays
        RenderPass() : RenderPass("Unnamed Render Pass (Thanks, bozo programmer)") { };

    private: 
        // for debug
        char name[STRING_MAX_SHORT] = {0};
    }; 

    /// @brief Base renderer class. Other renderers inherit from this
    class Renderer
    {
    public:
        virtual void Init() { };

        /// @brief Pumps the event queue for this renderer. Main Emulator and UI can be rendered before this.
        virtual void FramePreRender() { };

        /// @brief Performs the actual rendering.
        virtual void FramePostRender() { };
        virtual void Shutdown() { };

        // Getters for private fields
        int32_t GetWindowSizeX() { return windowSizeX; }; 
        int32_t GetWindowSizeY() { return windowSizeY; }; 
        RenderTexture* GetScreen() { return screen; };

        // Setters for private fields
        virtual void SetWindowSize(int32_t x, int32_t y) { };
        
        
        void AddRenderPass(RenderPass* pass)
        {
            Logger::Log(RENDER_LOG_PREFIX, std::format("Added render pass: {}", pass->GetName()).c_str(), LogChannels::Debug);
            passes.push_back(pass);
        }

        virtual Window& GetWindow() { return window; };

    protected: 
        int32_t windowSizeX = WINDOW_DEFAULT_SIZE_X, windowSizeY = WINDOW_DEFAULT_SIZE_Y;
        // vecotr of render passes
        std::vector<RenderPass*> passes = std::vector<RenderPass*>(); 
        RenderTexture* screen = nullptr;

    private: // don't exposed to derived classes as it will be a different type of window.
        Window window;
    };


}