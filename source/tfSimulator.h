/*******************************************************************************
 * This file is part of Tissue Forge.
 * Copyright (c) 2022, 2023 T.J. Sego
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 ******************************************************************************/

/**
 * @file tfSimulator.h
 * 
 */

#ifndef _SOURCE_TFSIMULATOR_H_
#define _SOURCE_TFSIMULATOR_H_

#include "TissueForge_private.h"

#include "io/tfFIO.h"
#ifdef TF_WITHCUDA
#include "cuda/tfSimulatorConfig.h"
#endif

#include "tfUniverse.h"

#include <cstdint>


namespace TissueForge {


    namespace rendering {
        struct Application;
        struct GlfwWindow;
    }


    /**
     * @brief The Simulator is the entry point to simulation, this is the very first object
     * that needs to be initialized  before any other method can be called. All the
     * methods of the Simulator are static, but the constructor needs to be called
     * first to initialize everything.
     *
     * The Simulator manages all of the operating system interface, it manages
     * window creation, end user input events, GPU access, threading, inter-process
     * messaging and so forth. All 'physical' modeling concepts go in the Universe.
     */
    struct CAPI_EXPORT Simulator {

        struct Config;
        class GLConfig;

        /**
         * @brief Window flag
         *
         * @see @ref WindowFlags, @ref setWindowFlags()
         */
        enum WindowFlags : std::uint16_t
        {
            /** Fullscreen window */
            Fullscreen = 1 << 0,

            /**
             * No window decoration
             */
            Borderless = 1 << 1,

            Resizable = 1 << 2,    /**< Resizable window */
            Hidden = 1 << 3,       /**< Hidden window */


            /**
             * Maximized window
             *
             * @note Supported since GLFW 3.2.
             */
            Maximized = 1 << 4,


            Minimized = 1 << 5,    /**< Minimized window */

            /**
             * Always on top
             * @m_since_latest
             */
            AlwaysOnTop = 1 << 6,



            /**
             * Automatically iconify (minimize) if fullscreen window loses
             * input focus
             */
            AutoIconify = 1 << 7,

            /**
             * Window has input focus
             *
             * @todo there's also GLFW_FOCUS_ON_SHOW, what's the difference?
             */
            Focused = 1 << 8,

            /**
             * Do not create any GPU context. Use together with
             * @ref GlfwApplication(const Arguments&),
             * @ref GlfwApplication(const Arguments&, const Configuration&),
             * @ref create(const Configuration&) or
             * @ref tryCreate(const Configuration&) to prevent implicit
             * creation of an OpenGL context.
             *
             * @note Supported since GLFW 3.2.
             */
            Contextless = 1 << 9

        };

        // Of the available integrator types, these are supported by Simulator
        enum class EngineIntegrator : int {
            FORWARD_EULER = TissueForge::EngineIntegrator::FORWARD_EULER,
            RUNGE_KUTTA_4 = TissueForge::EngineIntegrator::RUNGE_KUTTA_4
        };

        enum Key {
            NONE,
            WINDOWLESS,
            GLFW
        };

        enum Options {
            Windowless = 1 << 0,

            glfw = 1 << 1,
            /**
             * Forward compatible context
             *
             * @requires_gl Core/compatibility profile distinction and forward
             *      compatibility applies only to desktop GL.
             */
            GlForwardCompatible = 1 << 2,

            /**
             * Specifies whether errors should be generated by the context.
             * If enabled, situations that would have generated errors instead
             * cause undefined behavior.
             *
             * @note Supported since GLFW 3.2.
             */
            GlNoError = 1 << 3,


            /**
             * Debug context. Enabled automatically if the
             * `--magnum-gpu-validation` @ref GL-Context-command-line "command-line option"
             * is present.
             */
            GlDebug = 1 << 4,

            GlStereo = 1 << 5     /**< Stereo rendering */
        };

        enum class DpiScalingPolicy : std::uint8_t {
            /* Using 0 for an "unset" value */

            #ifdef CORRADE_TARGET_APPLE
            Framebuffer = 1,
            #endif

            #ifndef CORRADE_TARGET_APPLE
            Virtual = 2,

            Physical = 3,
            #endif

            Default
                #ifdef CORRADE_TARGET_APPLE
                = Framebuffer
                #else
                = Virtual
                #endif
        };

        int32_t kind;
        struct rendering::Application *app;

        enum Flags {
            Running = 1 << 0
        };

        /**
         * gets the global simulator object, returns NULL if fail.
         */
        static Simulator *get();

        /**
         * @brief Make the instance the global simulator object
         * 
         * @return HRESULT 
         */
        HRESULT makeCurrent();

        static HRESULT initConfig(const Config &conf, const GLConfig &glConf);

        /**
         * This function processes only those events that are already in the event
         * queue and then returns immediately. Processing events will cause the window
         * and input callbacks associated with those events to be called.
         *
         * On some platforms, a window move, resize or menu operation will cause
         * event processing to block. This is due to how event processing is designed
         * on those platforms. You can use the window refresh callback to redraw the
         * contents of your window when necessary during such operations.
         */
        static HRESULT pollEvents();

        /**
         *   This function puts the calling thread to sleep until at least one
         *   event is available in the event queue. Once one or more events are
         *   available, it behaves exactly like glfwPollEvents, i.e. the events
         *   in the queue are processed and the function then returns immediately.
         *   Processing events will cause the window and input callbacks associated
         *   with those events to be called.
         *
         *   Since not all events are associated with callbacks, this function may return
         *   without a callback having been called even if you are monitoring all callbacks.
         *
         *  On some platforms, a window move, resize or menu operation will cause event
         *  processing to block. This is due to how event processing is designed on
         *  those platforms. You can use the window refresh callback to redraw the
         *  contents of your window when necessary during such operations.
         */
        static HRESULT waitEvents();

        /**
         * This function puts the calling thread to sleep until at least
         * one event is available in the event queue, or until the specified
         * timeout is reached. If one or more events are available, it behaves
         * exactly like pollEvents, i.e. the events in the queue are
         * processed and the function then returns immediately. Processing
         * events will cause the window and input callbacks associated with those
         * events to be called.
         *
         * The timeout value must be a positive finite number.
         * Since not all events are associated with callbacks, this function may
         * return without a callback having been called even if you are monitoring
         * all callbacks.
         *
         * On some platforms, a window move, resize or menu operation will cause
         * event processing to block. This is due to how event processing is designed
         * on those platforms. You can use the window refresh callback to redraw the
         * contents of your window when necessary during such operations.
         */
        static HRESULT waitEventsTimeout(FloatP_t timeout);

        /**
         * This function posts an empty event from the current thread
         * to the event queue, causing waitEvents or waitEventsTimeout to return.
         */
        static HRESULT postEmptyEvent();

        /**
         * @brief Runs the event loop until all windows close or simulation time expires. 
         * Automatically performs universe time propogation. 
         * 
         * @param et final time; a negative number runs infinitely
         */
        static HRESULT run(FloatP_t et);

        /**
         * @brief Shows any windows that were specified in the config. 
         * Does not start the universe time propagation unlike ``run``.
         */
        static HRESULT show();

        /**
         * @brief Closes the main window, while the application / simulation continues to run.
         */
        static HRESULT close();

        static HRESULT destroy();

        /** Redraw the scene */
        static HRESULT redraw();

        /**
         * This function sets the swap interval for the current OpenGL or OpenGL ES context, i.e. the number of screen updates to wait from the time glfwSwapBuffers was called before swapping the buffers and returning. This is sometimes called vertical synchronization, vertical retrace synchronization or just vsync.
         * 
         * A context that supports either of the WGL_EXT_swap_control_tear and GLX_EXT_swap_control_tear extensions also accepts negative swap intervals, which allows the driver to swap immediately even if a frame arrives a little bit late. You can check for these extensions with glfwExtensionSupported.
         * 
         * A context must be current on the calling thread. Calling this function without a current context will cause a GLFW_NO_CURRENT_CONTEXT error.
         * 
         * This function does not apply to Vulkan. If you are rendering with Vulkan, see the present mode of your swapchain instead.
         * 
         * Parameters
         * [in]    interval    The minimum number of screen updates to wait for until the buffers are swapped by glfwSwapBuffers.
         * Errors
         * Possible errors include GLFW_NOT_INITIALIZED, GLFW_NO_CURRENT_CONTEXT and GLFW_PLATFORM_ERROR.
         * Remarks
         * This function is not called during context creation, leaving the swap interval set to whatever is the default on that platform. This is done because some swap interval extensions used by GLFW do not allow the swap interval to be reset to zero once it has been set to a non-zero value.
         * Some GPU drivers do not honor the requested swap interval, either because of a user setting that overrides the application's request or due to bugs in the driver.
         */
        static HRESULT swapInterval(int si);

        /** Number of threads */
        static const int getNumThreads();

        static const rendering::GlfwWindow *getWindow();

        #ifdef TF_WITHCUDA
        /**
         * @brief Get simulator CUDA runtime interface
         * 
         * @return cuda::SimulatorConfig* 
         */
        static cuda::SimulatorConfig *getCUDAConfig();

        /**
         * @brief Make a CUDA runtime interface instance the current global instance. 
         * 
         * Fails if already set.
         * 
         * @return HRESULT 
         */
        static HRESULT makeCUDAConfigCurrent(cuda::SimulatorConfig *config);
        #endif

        /** Set whether errors result in exceptions */
        static HRESULT throwExceptions(const bool &_throw);

        /** Check whether errors result in exceptions */
        static bool throwingExceptions();
        
        // list of windows.
        std::vector<rendering::GlfwWindow*> windows;
    };

    /**
     * @brief Test whether running interactively
     * 
     */
    CAPI_FUNC(bool) isTerminalInteractiveShell();

    /**
     * @brief Set whether running interactively
     */
    CAPI_FUNC(HRESULT) setIsTerminalInteractiveShell(const bool &_interactive);

    /**
     * main simulator init method
     */
    CPPAPI_FUNC(HRESULT) Simulator_init(const std::vector<std::string> &argv);

    /**
     * main simulator init method
     */
    CPPAPI_FUNC(HRESULT) Simulator_init(const Simulator::Config &conf, const std::vector<std::string> &appArgv=std::vector<std::string>());

    struct CAPI_EXPORT Simulator::Config {

        /*implicit*/
        Config();

        ~Config() {};

        /** @brief Window title */
        std::string title() const
        {
            return _title;
        }

        /**
         * @brief Set window title
         * @return Reference to self (for method chaining)
         *
         * Default is @cpp "Magnum GLFW Application" @ce.
         */
        void setTitle(std::string title)
        {
            _title = std::move(title);
        }

        /** @brief Window size */
        iVector2 windowSize() const
        {
            return _size;
        }

        /**
         * @brief DPI scaling policy
         *
         * If @ref dpiScaling() is non-zero, it has a priority over this value.
         * The `--magnum-dpi-scaling` command-line option has a priority over
         * any application-set value.
         * @see @ref setSize(const iVector2&, DpiScalingPolicy)
         */
        Simulator::DpiScalingPolicy dpiScalingPolicy() const
        {
            return _dpiScalingPolicy;
        }

        /**
         * @brief Custom DPI scaling
         *
         * If zero, then @ref dpiScalingPolicy() has a priority over this
         * value. The `--magnum-dpi-scaling` command-line option has a priority
         * over any application-set value.
         * @see @ref setSize(const iVector2&, const Vector2&)
         * @todo change this on a DPI change event (GLFW 3.3 has a callback:
         *  https://github.com/mosra/magnum/issues/243#issuecomment-388384089)
         */
        fVector2 dpiScaling() const
        {
            return _dpiScaling;
        }

        void setDpiScaling(const fVector2 &vec)
            {
                _dpiScaling = vec;
            }


        void setSizeAndScaling(const iVector2& size, Simulator::DpiScalingPolicy dpiScalingPolicy = Simulator::DpiScalingPolicy::Default) {
                    _size = size;
                    _dpiScalingPolicy = dpiScalingPolicy;

                }


        void setSizeAndScaling(const iVector2& size, const FVector2& dpiScaling) {
                    _size = size;
                    _dpiScaling = dpiScaling;
        }

        /**
         * @brief Set window size
         * @param size              Desired window size
         * @return Reference to self (for method chaining)
         *
         * Default is @cpp {800, 600} @ce. See @ref Platform-GlfwApplication-dpi
         * for more information.
         * @see @ref setSize(const iVector2&, const Vector2&)
         */
        void setWindowSize(const iVector2 &size)
        {
            _size = size;
        }

        /** @brief Window flags */
        uint32_t windowFlags() const
        {
            return _windowFlags;
        }

        /**
         * @brief Set window flags
         * @return  Reference to self (for method chaining)
         *
         * Default is @ref WindowFlag::Focused.
         */
        void setWindowFlags(uint32_t windowFlags)
        {
            _windowFlags = windowFlags;
        }

        bool windowless() const {
            return _windowless;
        }

        void setWindowless(bool val) {
            _windowless = val;
        }

        int size() const {
            return universeConfig.nParticles;
        }

        void setSize(int i ) {
            universeConfig.nParticles = i;
        }

        /** Random number generator seed */
        unsigned int *seed() {
            return _seed;
        }

        /** Set the random number generator seed */
        void setSeed(const unsigned int &seed) {
            if(_seed != NULL) *_seed = seed;
            else _seed = new unsigned int(seed);
        }

        bool throwingExceptions() const {
            return _throwingExceptions;
        }

        void setThrowingExceptions(const bool &throwingExceptions) {
            _throwingExceptions = throwingExceptions;
        }

        /** Universe configuration */
        UniverseConfig universeConfig;

        int queues;

        int argc = 0;

        char** argv = NULL;
        
        std::string *importDataFilePath = NULL;
        /* Imported data file path during initialization, if any */
        
        /** Clip planes to implement in visualization */
        std::vector<FVector4> clipPlanes;

    private:
        std::string _title;
        iVector2 _size;
        uint32_t _windowFlags;
        Simulator::DpiScalingPolicy _dpiScalingPolicy;
        FVector2 _dpiScaling;
        bool _windowless;
        unsigned int *_seed = NULL;
        bool _throwingExceptions = false;
    };

    /**
     @brief OpenGL context configuration

    The created window is always with a double-buffered OpenGL context.

    @note This function is available only if Magnum is compiled with
    @ref MAGNUM_TARGET_GL enabled (done by default). See @ref building-features
    for more information.

    @see @ref GlfwApplication(), @ref create(), @ref tryCreate()
    */
    class CAPI_EXPORT Simulator::GLConfig {
    public:
        /**
         * @brief Context flag
         *
         * @see @ref Flags, @ref setFlags(), @ref GL::Context::Flag
         */
        enum  Flag: uint32_t {
    #ifndef MAGNUM_TARGET_GLES
            /**
             * Forward compatible context
             *
             * @requires_gl Core/compatibility profile distinction and forward
             *      compatibility applies only to desktop GL.
             */
            ForwardCompatible = 1 << 0,
    #endif

    #if defined(DOXYGEN_GENERATING_OUTPUT) || defined(GLFW_CONTEXT_NO_ERROR)
            /**
             * Specifies whether errors should be generated by the context.
             * If enabled, situations that would have generated errors instead
             * cause undefined behavior.
             *
             * @note Supported since GLFW 3.2.
             */
            NoError = 1 << 1,
    #endif

            /**
             * Debug context. Enabled automatically if the
             * `--magnum-gpu-validation` @ref GL-Context-command-line "command-line option"
             * is present.
             */
            Debug = 1 << 2,

            Stereo = 1 << 3     /**< Stereo rendering */
        };

        /**
         * @brief Context flags
         *
         * @see @ref setFlags(), @ref GL::Context::Flags
         */
        typedef uint32_t Flags;

        explicit GLConfig();
        ~GLConfig();

        /** @brief Context flags */
        Flags flags() const { return _flags; }

        /**
         * @brief Set context flags
         * @return Reference to self (for method chaining)
         *
         * Default is @ref Flag::ForwardCompatible on desktop GL and no flags
         * on OpenGL ES.
         * @see @ref addFlags(), @ref clearFlags(), @ref GL::Context::flags()
         */
        GLConfig& setFlags(Flags flags) {
            _flags = flags;
            return *this;
        }

        /**
         * @brief Add context flags
         * @return Reference to self (for method chaining)
         *
         * Unlike @ref setFlags(), ORs the flags with existing instead of
         * replacing them. Useful for preserving the defaults.
         * @see @ref clearFlags()
         */
        GLConfig& addFlags(Flags flags) {
            _flags |= flags;
            return *this;
        }

        /**
         * @brief Clear context flags
         * @return Reference to self (for method chaining)
         *
         * Unlike @ref setFlags(), ANDs the inverse of @p flags with existing
         * instead of replacing them. Useful for removing default flags.
         * @see @ref addFlags()
         */
        GLConfig& clearFlags(Flags flags) {
            _flags &= ~flags;
            return *this;
        }

        /** @brief Context version */
        std::int32_t version() const { return _version; }

        /**
         * @brief Set context version
         *
         * If requesting version greater or equal to OpenGL 3.2, core profile
         * is used. The created context will then have any version which is
         * backwards-compatible with requested one. Default is
         * @ref GL::Version::None, i.e. any provided version is used.
         */
        GLConfig& setVersion(std::int32_t version) {
            _version = version;
            return *this;
        }

        /** @brief Color buffer size */
        iVector4 colorBufferSize() const { return _colorBufferSize; }

        /**
         * @brief Set color buffer size
         *
         * Default is @cpp {8, 8, 8, 0} @ce (8-bit-per-channel RGB, no alpha).
         * @see @ref setDepthBufferSize(), @ref setStencilBufferSize()
         */
        GLConfig& setColorBufferSize(const iVector4& size) {
            _colorBufferSize = size;
            return *this;
        }

        /** @brief Depth buffer size */
        std::int32_t depthBufferSize() const { return _depthBufferSize; }

        /**
         * @brief Set depth buffer size
         *
         * Default is @cpp 24 @ce bits.
         * @see @ref setColorBufferSize(), @ref setStencilBufferSize()
         */
        GLConfig& setDepthBufferSize(std::int32_t size) {
            _depthBufferSize = size;
            return *this;
        }

        /** @brief Stencil buffer size */
        std::int32_t stencilBufferSize() const { return _stencilBufferSize; }

        /**
         * @brief Set stencil buffer size
         *
         * Default is @cpp 0 @ce bits (i.e., no stencil buffer).
         * @see @ref setColorBufferSize(), @ref setDepthBufferSize()
         */
        GLConfig& setStencilBufferSize(std::int32_t size) {
            _stencilBufferSize = size;
            return *this;
        }

        /** @brief Sample count */
        std::int32_t sampleCount() const { return _sampleCount; }

        /**
         * @brief Set sample count
         * @return Reference to self (for method chaining)
         *
         * Default is @cpp 0 @ce, thus no multisampling. The actual sample
         * count is ignored, GLFW either enables it or disables. See also
         * @ref GL::Renderer::Feature::Multisampling.
         */
        GLConfig& setSampleCount(std::int32_t count) {
            _sampleCount = count;
            return *this;
        }

        /** @brief sRGB-capable default framebuffer */
        bool isSrgbCapable() const { return _srgbCapable; }

        /**
         * @brief Set sRGB-capable default framebuffer
         *
         * Default is @cpp false @ce. See also
         * @ref GL::Renderer::Feature::FramebufferSrgb.
         * @return Reference to self (for method chaining)
         */
        GLConfig& setSrgbCapable(bool enabled) {
            _srgbCapable = enabled;
            return *this;
        }


    private:
        iVector4 _colorBufferSize;
        std::int32_t _depthBufferSize, _stencilBufferSize;
        std::int32_t _sampleCount;
        std::int32_t _version;
        Flags _flags;
        bool _srgbCapable;
    };

    HRESULT initSimConfigFromFile(const std::string &loadFilePath, Simulator::Config &conf);

    CAPI_FUNC(HRESULT) universe_init(const UniverseConfig &conf);

    CAPI_FUNC(HRESULT) modules_init();


    namespace io {

        template <>
        HRESULT toFile(const Simulator &dataElement, const MetaData &metaData, IOElement &fileElement);

        template <>
        HRESULT fromFile(const IOElement &fileElement, const MetaData &metaData, Simulator::Config *dataElement);

    }

};

#endif // _SOURCE_TFSIMULATOR_H_
