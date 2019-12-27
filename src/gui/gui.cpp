#include "gui/gui.hpp"
#include "system/system.hpp"
#include "system/local_file_system.hpp"
#include "gui/window.hpp"

namespace igx::ui {

	GUI::GUI(Graphics &g, const SetClearColor &clearColor, usz commandListSize) :
		clearColor(clearColor), commandListSize(commandListSize),
		flags(Flags(Flags::OWNS_COMMAND_LIST | Flags::OWNS_FRAMEBUFFER))
	{
		init(g);
	}

	GUI::GUI(Graphics &g, const Framebuffer &fb, usz commandListSize) :
		target(fb), commandListSize(commandListSize), flags(Flags::OWNS_COMMAND_LIST)
	{
		init(g);
	}

	GUI::GUI(Graphics &g, const CommandList &cl) :
		commands(cl), flags(Flags::OWNS_FRAMEBUFFER)
	{
		init(g);
	}

	GUI::GUI(Graphics &g, const Framebuffer &fb, const CommandList &cl) :
		target(fb), commands(cl), flags(Flags::NONE)
	{
		init(g);
	}

	void GUI::init(Graphics &g) {

		resolution = {
			g, NAME("GUI resolution buffer"),
			GPUBuffer::Info(
				8, GPUBufferType::UNIFORM, GPUMemoryUsage::CPU_WRITE
			)
		};

		sampler = {
			g, NAME("GUI sampler"),
			Sampler::Info()
		};

		Buffer vertShader, fragShader;
		oicAssert("Couldn't find pass through vertex shader", oic::System::files()->read("./shaders/pass_through.vert.spv", vertShader));
		oicAssert("Couldn't find pass through fragment shader", oic::System::files()->read("./shaders/pass_through.frag.spv", fragShader));

		bool hasFb = !(flags & Flags::OWNS_FRAMEBUFFER);

		uiShader = {
			g, NAME("GUI pipeline"),
			Pipeline::Info(
				PipelineFlag::OPTIMIZE,
				{ vertexLayout },
				{
					{ ShaderStage::VERTEX, vertShader },
					{ ShaderStage::FRAGMENT, fragShader }
				},
				pipelineLayout,
				MSAA(hasFb ? target->getInfo().samples : msaa, .2f),
				DepthStencil(),
				Rasterizer(CullMode::NONE),
				BlendState::alphaBlend()
			)
		};

		if (flags & Flags::OWNS_COMMAND_LIST)
			commands = {
				g, NAME("GUI command list"),
				CommandList::Info(commandListSize)
			};

		if (flags & Flags::OWNS_FRAMEBUFFER)
			target = {
				g, NAME("GUI framebuffer"),
				Framebuffer::Info(
					{ GPUFormat::RGBA8 },
					DepthFormat::NONE,
					false,
					msaa
				)
			};

		initData(g);
	}

	void GUI::resize(const Vec2u32 &size) {

		requestUpdate();

		if (flags & GUI::OWNS_FRAMEBUFFER)
			target->onResize(size);

		memcpy(resolution->getBuffer(), size.arr, sizeof(size));
		resolution->flush(0, 8);

		//TODO: UIWindows
	}

	void GUI::requestUpdate() {
		shouldRefresh = couldRefresh = true;
	}

	void GUI::beginDraw() {

		if (flags & GUI::OWNS_COMMAND_LIST)
			commands->clear();

		commands->add(
			BindPipeline(uiShader),
			clearColor,
			BeginFramebuffer(target),
			BindDescriptors(descriptors),
			SetViewportAndScissor()
		);

		if (flags & GUI::OWNS_FRAMEBUFFER)
			commands->add(ClearFramebuffer(target));

	}

	void GUI::endDraw() {
		commands->add(EndFramebuffer());
	}

	void GUI::render(Graphics &g) {

		if (shouldRefresh || couldRefresh) {

			//Check if a graphical change occurred by checking via UI

			if (prepareDrawData()) {
				bakePrimitives(g);
				shouldRefresh = true;
			}

			//Only draw if it should

			if (shouldRefresh) {
				beginDraw();
				draw();
				endDraw();
			}

			//Reset so it only draws when needed
			shouldRefresh = couldRefresh = false;

		}
	}

}