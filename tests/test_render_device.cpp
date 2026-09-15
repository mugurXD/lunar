#include <lunar/render/render_device.hpp>
#include <lunar/render/mesh_registry.hpp>
#include <lunar/file/binary_file.hpp>
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

using namespace lunar::Render;

namespace
{
	constexpr std::string_view SHADER_EXTENSION    = ".spv";
	constexpr uint32_t         VALUE_COUNT         = 1000;
	constexpr size_t           VALUES_SIZE         = VALUE_COUNT * sizeof(uint32_t);
	constexpr uint32_t         WORKGROUP_SIZE      = 64;
	constexpr uint32_t         FILL_MULTIPLIER     = 3;
	constexpr uint32_t         FILL_OFFSET         = 7;
	constexpr uint32_t         ADD_OFFSET          = 1;
	constexpr uint32_t         PATCH_INDEX         = 256;
	constexpr uint32_t         PATCH_COUNT         = 16;
	constexpr uint32_t         PATCH_VALUE         = 0xABCD;
	constexpr size_t           SMALL_BUFFER_SIZE   = 64;
	constexpr int              FRAME_COUNT         = 10;
	constexpr uint32_t         UPLOAD_BATCH_ROUNDS = 10;
	constexpr Extent2D         IMAGE_EXTENT        = { 64, 32 };
	constexpr uint32_t         TRIANGLE_VERTICES   = 3;
	constexpr size_t           TRANSIENT_ODD_SIZE  = 13;
	constexpr size_t           TRANSIENT_OVERSIZED = size_t(1) << 30;

	constexpr uint32_t TRANSIENT_ALLOCATIONS_PER_FRAME = 10000;

	struct ComputeConstants
	{
		uint64_t source      = 0;
		uint64_t destination = 0;
		uint32_t count       = 0;
		uint32_t multiplier  = 0;
		uint32_t offset      = 0;
	};

	std::vector<char> LoadShader(std::string_view name)
	{
		return Fs::BinaryFile(Fs::Path(LUNAR_TEST_SHADER_DIR) / (std::string(name) + std::string(SHADER_EXTENSION))).content;
	}

	std::vector<uint32_t> Sequence(uint32_t multiplier, uint32_t offset)
	{
		std::vector<uint32_t> values(VALUE_COUNT);
		for (uint32_t index = 0; index < VALUE_COUNT; index++)
			values[index] = index * multiplier + offset;

		return values;
	}

	BufferDesc ValuesBuffer(MemoryLocation location)
	{
		return { VALUES_SIZE, BufferUsageFlags(BufferUsageFlagBits::eStorage), location };
	}

	uint32_t GroupCount(uint32_t count)
	{
		return (count + WORKGROUP_SIZE - 1) / WORKGROUP_SIZE;
	}

	ImageDesc DepthImage()
	{
		return { .extent = IMAGE_EXTENT, .format = Format::eD32Float, .usage = ImageUsageFlags(ImageUsageFlagBits::eDepthAttachment) };
	}
}

class RenderDeviceTest : public ::testing::Test
{
protected:
	static void SetUpTestSuite()
	{
		device = CreateRenderDevice({ .appName = "lunar_render_tests" });
	}

	static void TearDownTestSuite()
	{
		device.reset();
	}

	void SetUp() override
	{
		device->waitIdle();
		baseline = device->getStats();
	}

	void TearDown() override
	{
		device->waitIdle();
		const RenderDeviceStats stats = device->getStats();

		EXPECT_EQ(stats.bufferCount,             baseline.bufferCount)          << "Leaked buffer handles";
		EXPECT_EQ(stats.imageCount,              baseline.imageCount)           << "Leaked image handles";
		EXPECT_EQ(stats.pipelineCount,           baseline.pipelineCount)        << "Leaked pipeline handles";
		EXPECT_EQ(stats.pendingDestructionCount, 0u)                            << "Destructions still pending after waitIdle";
		EXPECT_EQ(stats.allocationCount,         baseline.allocationCount)      << "Leaked GPU allocations";
		EXPECT_EQ(stats.allocationBytes,         baseline.allocationBytes)      << "Leaked GPU memory";
		EXPECT_EQ(stats.validationErrorCount,    baseline.validationErrorCount) << "Vulkan validation errors were reported";
	}

	PipelineHandle createCompute(std::string_view shader_name)
	{
		const std::vector<char> shader = LoadShader(shader_name);
		return device->createComputePipeline({ std::as_bytes(std::span(shader)) });
	}

	void runCompute(PipelineHandle pipeline, const ComputeConstants& constants)
	{
		Frame&       frame    = device->beginFrame();
		CommandList& commands = frame.commandList();

		commands.bindPipeline(pipeline);
		commands.pushConstants(constants);
		commands.dispatch(GroupCount(constants.count));

		device->endFrame(frame);
	}

	std::vector<uint32_t> readValues(BufferHandle buffer)
	{
		device->waitIdle();

		const std::span<const std::byte> bytes = device->readBuffer(buffer);
		std::vector<uint32_t>            values(bytes.size() / sizeof(uint32_t));
		std::memcpy(values.data(), bytes.data(), values.size() * sizeof(uint32_t));

		return values;
	}

	std::vector<uint32_t> downloadValues(BufferHandle gpu_buffer)
	{
		const PipelineHandle copy     = createCompute("copy.comp");
		const BufferHandle   readback = device->createBuffer(ValuesBuffer(MemoryLocation::eReadback), {});

		runCompute(copy, {
			.source      = device->getBufferAddress(gpu_buffer),
			.destination = device->getBufferAddress(readback),
			.count       = VALUE_COUNT
		});

		const std::vector<uint32_t> values = readValues(readback);

		device->destroyBuffer(readback);
		device->destroyPipeline(copy);
		return values;
	}

	static inline std::unique_ptr<RenderDevice> device;
	RenderDeviceStats                            baseline = {};
};

TEST_F(RenderDeviceTest, GpuOnlyStorageBufferHasDeviceAddress)
{
	const BufferHandle buffer = device->createBuffer(ValuesBuffer(MemoryLocation::eGpuOnly), {});

	EXPECT_NE(buffer, BufferHandle {});
	EXPECT_NE(device->getBufferAddress(buffer), 0u);
	EXPECT_EQ(device->getStats().bufferCount,     baseline.bufferCount + 1);
	EXPECT_EQ(device->getStats().allocationCount, baseline.allocationCount + 1);

	device->destroyBuffer(buffer);
}

TEST_F(RenderDeviceTest, UniformOnlyBufferHasNoDeviceAddress)
{
	const BufferHandle buffer = device->createBuffer({ SMALL_BUFFER_SIZE, BufferUsageFlags(BufferUsageFlagBits::eUniform), MemoryLocation::eUpload }, {});

	EXPECT_EQ(device->getBufferAddress(buffer), 0u);

	device->destroyBuffer(buffer);
}

TEST_F(RenderDeviceTest, DestroyedBufferHandleBecomesStale)
{
	const BufferHandle buffer = device->createBuffer(ValuesBuffer(MemoryLocation::eReadback), {});
	device->destroyBuffer(buffer);

	EXPECT_EQ(device->getBufferAddress(buffer), 0u);
	EXPECT_TRUE(device->readBuffer(buffer).empty());

	device->destroyBuffer(buffer);
	EXPECT_EQ(device->getStats().bufferCount, baseline.bufferCount);
}

TEST_F(RenderDeviceTest, ReusedSlotDoesNotMatchStaleHandle)
{
	const BufferHandle stale = device->createBuffer(ValuesBuffer(MemoryLocation::eGpuOnly), {});
	device->destroyBuffer(stale);

	const BufferHandle reused = device->createBuffer(ValuesBuffer(MemoryLocation::eGpuOnly), {});

	EXPECT_NE(reused, stale);
	EXPECT_EQ(device->getBufferAddress(stale), 0u);
	EXPECT_NE(device->getBufferAddress(reused), 0u);

	device->destroyBuffer(reused);
}

TEST_F(RenderDeviceTest, MappedUploadIsCompleteImmediately)
{
	const std::vector<uint32_t> values = Sequence(FILL_MULTIPLIER, FILL_OFFSET);
	const BufferHandle          buffer = device->createBuffer(ValuesBuffer(MemoryLocation::eUpload), {});
	const UploadTicket          ticket = device->uploadBuffer(buffer, 0, std::as_bytes(std::span(values)));

	EXPECT_TRUE(device->isComplete(ticket));
	EXPECT_EQ(readValues(buffer), values);

	device->destroyBuffer(buffer);
}

TEST_F(RenderDeviceTest, StagedUploadCompletesOnlyAfterFlush)
{
	const std::vector<uint32_t> values = Sequence(FILL_MULTIPLIER, FILL_OFFSET);
	const BufferHandle          buffer = device->createBuffer(ValuesBuffer(MemoryLocation::eGpuOnly), {});
	const UploadTicket          ticket = device->uploadBuffer(buffer, 0, std::as_bytes(std::span(values)));

	EXPECT_FALSE(device->isComplete(ticket));

	device->flushUploads();
	device->waitIdle();
	EXPECT_TRUE(device->isComplete(ticket));

	device->destroyBuffer(buffer);
}

TEST_F(RenderDeviceTest, StagedUploadsReachGpuInOrder)
{
	const std::vector<uint32_t> values = Sequence(FILL_MULTIPLIER, FILL_OFFSET);
	const std::vector<uint32_t> patch(PATCH_COUNT, PATCH_VALUE);

	const BufferHandle buffer = device->createBuffer(ValuesBuffer(MemoryLocation::eGpuOnly), std::as_bytes(std::span(values)));
	device->uploadBuffer(buffer, PATCH_INDEX * sizeof(uint32_t), std::as_bytes(std::span(patch)));

	std::vector<uint32_t> expected = values;
	std::ranges::copy(patch, expected.begin() + PATCH_INDEX);

	EXPECT_EQ(downloadValues(buffer), expected);

	device->destroyBuffer(buffer);
}

TEST_F(RenderDeviceTest, UploadsAcrossManyBatchesAllComplete)
{
	const BufferHandle buffer = device->createBuffer(ValuesBuffer(MemoryLocation::eGpuOnly), {});
	UploadTicket       last   = {};

	for (uint32_t round = 0; round < UPLOAD_BATCH_ROUNDS; round++)
	{
		const std::vector<uint32_t> values = Sequence(round, FILL_OFFSET);
		device->uploadBuffer(buffer, 0, std::as_bytes(std::span(values)));
		last = device->flushUploads();
	}

	device->waitIdle();
	EXPECT_TRUE(device->isComplete(last));
	EXPECT_EQ(downloadValues(buffer), Sequence(UPLOAD_BATCH_ROUNDS - 1, FILL_OFFSET));

	device->destroyBuffer(buffer);
}

TEST_F(RenderDeviceTest, BufferDestroyedWithPendingUploadIsDeferred)
{
	const std::vector<uint32_t> values = Sequence(FILL_MULTIPLIER, FILL_OFFSET);
	const BufferHandle          buffer = device->createBuffer(ValuesBuffer(MemoryLocation::eGpuOnly), std::as_bytes(std::span(values)));

	device->destroyBuffer(buffer);

	EXPECT_EQ(device->getStats().bufferCount,             baseline.bufferCount);
	EXPECT_EQ(device->getStats().pendingDestructionCount, 1u);
}

TEST_F(RenderDeviceTest, HeadlessFramesSubmitWithoutSwapchain)
{
	for (int frame_index = 0; frame_index < FRAME_COUNT; frame_index++)
		device->endFrame(device->beginFrame());
}

TEST_F(RenderDeviceTest, BufferDestroyedDuringFrameIsReleasedAfterIt)
{
	const BufferHandle buffer = device->createBuffer(ValuesBuffer(MemoryLocation::eGpuOnly), {});

	Frame& frame = device->beginFrame();
	device->destroyBuffer(buffer);
	EXPECT_EQ(device->getStats().pendingDestructionCount, 1u);

	device->endFrame(frame);
	device->waitIdle();
	EXPECT_EQ(device->getStats().pendingDestructionCount, 0u);
}

TEST_F(RenderDeviceTest, InvalidShaderBytecodeReturnsNullPipeline)
{
	EXPECT_EQ(device->createComputePipeline({}),  PipelineHandle {});
	EXPECT_EQ(device->createGraphicsPipeline({}), PipelineHandle {});
	EXPECT_EQ(device->getStats().pipelineCount,   baseline.pipelineCount);
}

TEST_F(RenderDeviceTest, GraphicsPipelineSupportsCommonFormats)
{
	const std::vector<char> vertex_shader   = LoadShader("triangle.vert");
	const std::vector<char> fragment_shader = LoadShader("white.frag");

	for (const Format color_format : { Format::eRGBA8Unorm, Format::eBGRA8Srgb, Format::eRGBA16Float })
	{
		const PipelineHandle pipeline = device->createGraphicsPipeline({
			.vertexShader   = std::as_bytes(std::span(vertex_shader)),
			.fragmentShader = std::as_bytes(std::span(fragment_shader)),
			.colorFormats   = std::span(&color_format, 1),
			.depthFormat    = Format::eD32Float,
			.depthTest      = true,
			.depthWrite     = true,
			.blendMode      = BlendMode::eAlpha
		});

		EXPECT_NE(pipeline, PipelineHandle {});
		device->destroyPipeline(pipeline);
	}
}

TEST_F(RenderDeviceTest, ComputeWritesAreVisibleAfterBarrier)
{
	const PipelineHandle fill = createCompute("fill.comp");
	const PipelineHandle add  = createCompute("add.comp");
	ASSERT_NE(fill, PipelineHandle {});
	ASSERT_NE(add,  PipelineHandle {});

	const BufferHandle buffer  = device->createBuffer(ValuesBuffer(MemoryLocation::eReadback), {});
	const uint64_t     address = device->getBufferAddress(buffer);

	Frame&       frame    = device->beginFrame();
	CommandList& commands = frame.commandList();

	commands.bindPipeline(fill);
	commands.pushConstants(ComputeConstants { .destination = address, .count = VALUE_COUNT, .multiplier = FILL_MULTIPLIER, .offset = FILL_OFFSET });
	commands.dispatch(GroupCount(VALUE_COUNT));
	commands.memoryBarrier();
	commands.bindPipeline(add);
	commands.pushConstants(ComputeConstants { .destination = address, .count = VALUE_COUNT, .offset = ADD_OFFSET });
	commands.dispatch(GroupCount(VALUE_COUNT));

	device->endFrame(frame);

	EXPECT_EQ(readValues(buffer), Sequence(FILL_MULTIPLIER, FILL_OFFSET + ADD_OFFSET));

	device->destroyBuffer(buffer);
	device->destroyPipeline(fill);
	device->destroyPipeline(add);
}

TEST_F(RenderDeviceTest, DepthImageReportsItsExtent)
{
	const ImageHandle image = device->createImage(DepthImage());

	EXPECT_NE(image, ImageHandle {});
	EXPECT_EQ(device->getImageExtent(image),      IMAGE_EXTENT);
	EXPECT_EQ(device->getStats().imageCount,      baseline.imageCount + 1);
	EXPECT_EQ(device->getStats().allocationCount, baseline.allocationCount + 1);

	device->destroyImage(image);
}

TEST_F(RenderDeviceTest, InvalidImageDescReturnsNullImage)
{
	EXPECT_EQ(device->createImage({}), ImageHandle {});
	EXPECT_EQ(device->createImage({ .extent = IMAGE_EXTENT }), ImageHandle {});
	EXPECT_EQ(device->createImage({ .extent = IMAGE_EXTENT, .format = Format::eRGBA8Unorm, .mipLevels = 0 }), ImageHandle {});
	EXPECT_EQ(device->getStats().imageCount, baseline.imageCount);
}

TEST_F(RenderDeviceTest, DestroyedImageHandleBecomesStale)
{
	const ImageHandle image = device->createImage(DepthImage());
	device->destroyImage(image);

	EXPECT_EQ(device->getImageExtent(image), Extent2D {});

	device->destroyImage(image);
	EXPECT_EQ(device->getStats().imageCount, baseline.imageCount);
}

TEST_F(RenderDeviceTest, RenderingToColorAndDepthAcrossFrames)
{
	const std::vector<char> vertex_shader   = LoadShader("triangle.vert");
	const std::vector<char> fragment_shader = LoadShader("white.frag");
	const Format            color_format    = Format::eRGBA8Unorm;

	const ImageHandle color = device->createImage({
		.extent = IMAGE_EXTENT,
		.format = color_format,
		.usage  = ImageUsageFlags(ImageUsageFlagBits::eColorAttachment)
	});
	const ImageHandle    depth    = device->createImage(DepthImage());
	const PipelineHandle pipeline = device->createGraphicsPipeline({
		.vertexShader   = std::as_bytes(std::span(vertex_shader)),
		.fragmentShader = std::as_bytes(std::span(fragment_shader)),
		.colorFormats   = std::span(&color_format, 1),
		.depthFormat    = Format::eD32Float,
		.depthTest      = true,
		.depthWrite     = true,
		.depthCompare   = CompareOp::eGreaterOrEqual
	});
	ASSERT_NE(pipeline, PipelineHandle {});

	const ColorAttachment color_attachment = { .image = color };
	const DepthAttachment depth_attachment = { .image = depth };

	for (int frame_index = 0; frame_index < FRAME_COUNT; frame_index++)
	{
		Frame&       frame    = device->beginFrame();
		CommandList& commands = frame.commandList();

		commands.beginRendering({ .colorAttachments = std::span(&color_attachment, 1), .depthAttachment = depth_attachment });
		commands.bindPipeline(pipeline);
		commands.draw(TRIANGLE_VERTICES);
		commands.endRendering();

		device->endFrame(frame);
	}

	device->destroyPipeline(pipeline);
	device->destroyImage(depth);
	device->destroyImage(color);
}

TEST_F(RenderDeviceTest, MeshRegistryCreatesCubeMesh)
{
	MeshRegistry     registry(*device);
	const MeshHandle cube = registry.create(CreateCubeMeshData());
	const Mesh*      mesh = registry.get(cube);

	ASSERT_NE(mesh, nullptr);
	EXPECT_EQ(mesh->indexCount, CreateCubeMeshData().indices.size());
	EXPECT_NE(mesh->vertexAddress, 0u);
	EXPECT_EQ(registry.size(), 1u);
	EXPECT_EQ(device->getStats().bufferCount, baseline.bufferCount + 2);
}

TEST_F(RenderDeviceTest, DestroyedMeshHandleBecomesStale)
{
	MeshRegistry     registry(*device);
	const MeshHandle cube = registry.create(CreateCubeMeshData());

	registry.destroy(cube);
	EXPECT_EQ(registry.get(cube), nullptr);

	registry.destroy(cube);
	EXPECT_EQ(registry.size(), 0u);
	EXPECT_EQ(device->getStats().bufferCount, baseline.bufferCount);
}

TEST_F(RenderDeviceTest, EmptyMeshDataReturnsNullMesh)
{
	MeshRegistry registry(*device);

	EXPECT_EQ(registry.create({}), MeshHandle {});
	EXPECT_EQ(registry.size(), 0u);
	EXPECT_EQ(device->getStats().bufferCount, baseline.bufferCount);
}

TEST_F(RenderDeviceTest, TransientAllocationsAreAlignedAndDistinct)
{
	Frame& frame = device->beginFrame();

	const TransientAllocation first  = frame.allocateTransient(TRANSIENT_ODD_SIZE);
	const TransientAllocation second = frame.allocateTransient(TRANSIENT_ODD_SIZE);

	EXPECT_EQ(first.data.size(), TRANSIENT_ODD_SIZE);
	EXPECT_NE(first.address, 0u);
	EXPECT_EQ(first.address  % TRANSIENT_ALIGNMENT, 0u);
	EXPECT_EQ(second.address % TRANSIENT_ALIGNMENT, 0u);
	EXPECT_GE(second.address, first.address + TRANSIENT_ODD_SIZE);
	EXPECT_GE(second.data.data(), first.data.data() + TRANSIENT_ODD_SIZE);

	device->endFrame(frame);
}

TEST_F(RenderDeviceTest, OversizedTransientAllocationFails)
{
	Frame& frame = device->beginFrame();

	const TransientAllocation allocation = frame.allocateTransient(TRANSIENT_OVERSIZED);

	EXPECT_TRUE(allocation.data.empty());
	EXPECT_EQ(allocation.address, 0u);

	device->endFrame(frame);
}

TEST_F(RenderDeviceTest, TransientDataIsReadableByGpu)
{
	const std::vector<uint32_t> values   = Sequence(FILL_MULTIPLIER, FILL_OFFSET);
	const PipelineHandle        copy     = createCompute("copy.comp");
	const BufferHandle          readback = device->createBuffer(ValuesBuffer(MemoryLocation::eReadback), {});
	ASSERT_NE(copy, PipelineHandle {});

	Frame&                    frame      = device->beginFrame();
	const TransientAllocation allocation = frame.allocateTransient(VALUES_SIZE);
	ASSERT_EQ(allocation.data.size(), VALUES_SIZE);
	std::memcpy(allocation.data.data(), values.data(), VALUES_SIZE);

	CommandList& commands = frame.commandList();
	commands.bindPipeline(copy);
	commands.pushConstants(ComputeConstants { .source = allocation.address, .destination = device->getBufferAddress(readback), .count = VALUE_COUNT });
	commands.dispatch(GroupCount(VALUE_COUNT));
	device->endFrame(frame);

	EXPECT_EQ(readValues(readback), values);

	device->destroyBuffer(readback);
	device->destroyPipeline(copy);
}

TEST_F(RenderDeviceTest, TransientMemoryDoesNotGrowAcrossFrames)
{
	for (int frame_index = 0; frame_index < FRAME_COUNT; frame_index++)
	{
		Frame& frame = device->beginFrame();
		for (uint32_t allocation_index = 0; allocation_index < TRANSIENT_ALLOCATIONS_PER_FRAME; allocation_index++)
			EXPECT_NE(frame.writeTransient(allocation_index), 0u);

		device->endFrame(frame);
	}

	device->waitIdle();
	EXPECT_EQ(device->getStats().allocationCount, baseline.allocationCount);
	EXPECT_EQ(device->getStats().allocationBytes, baseline.allocationBytes);
}

TEST(RenderDeviceLifetime, DestroyingDeviceReleasesLiveResources)
{
	std::unique_ptr<RenderDevice> owned_device = CreateRenderDevice({ .appName = "lunar_render_tests_lifetime" });

	const std::vector<uint32_t> values  = Sequence(FILL_MULTIPLIER, FILL_OFFSET);
	const std::vector<char>     shader  = LoadShader("fill.comp");
	const BufferHandle          staged  = owned_device->createBuffer(ValuesBuffer(MemoryLocation::eGpuOnly), std::as_bytes(std::span(values)));
	const BufferHandle          mapped  = owned_device->createBuffer(ValuesBuffer(MemoryLocation::eReadback), {});
	const PipelineHandle        compute = owned_device->createComputePipeline({ std::as_bytes(std::span(shader)) });
	const ImageHandle           depth   = owned_device->createImage(DepthImage());

	owned_device->destroyBuffer(owned_device->createBuffer(ValuesBuffer(MemoryLocation::eGpuOnly), std::as_bytes(std::span(values))));
	owned_device->endFrame(owned_device->beginFrame());

	EXPECT_NE(staged,  BufferHandle {});
	EXPECT_NE(mapped,  BufferHandle {});
	EXPECT_NE(compute, PipelineHandle {});
	EXPECT_NE(depth,   ImageHandle {});

	owned_device.reset();
}
