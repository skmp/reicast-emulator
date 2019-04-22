// ps4_video.h
//

#define DISPLAY_BUFFER_NUM 2 // minimum 2 required
#define FLIP_RATE 0 // 0: no fliprate is set. 1: 30fps on 60Hz video output  2: 20fps
#define FLIP_MODE SCE_VIDEO_OUT_FLIP_MODE_VSYNC // SceVideoOutFlipMode

#define VIDEO_MEMORY_STACK_SIZE (1024 * 1024 * 192)

#define SLEEP_BEFORE_FINISH 2 // in seconds
#define LINE_INTERVAL 200 // interval of the checker board on screen in pixels



#define assert verify


void ps4_video_init();


// display buffer attributes
class RenderBufferAttributes {
public:
	void init(int width, int height = 0, bool isTile = true)
	{
		// set basic parameters
		m_width = width;
		if (height == 0) m_height = m_width * 9 / 16;
		else m_height = height;
		m_pixelFormat = SCE_VIDEO_OUT_PIXEL_FORMAT_A8R8G8B8_SRGB;
		m_dataFormat = Gnm::kDataFormatB8G8R8A8UnormSrgb;
		m_bpp = sizeof(uint32_t);
		m_isTile = isTile;

		// set other parameters
		Gnm::RenderTargetSpec rtSpec;
		rtSpec.init();
		rtSpec.m_width = m_width;
		rtSpec.m_height = m_height;
		rtSpec.m_pitch = 0;
		rtSpec.m_numSlices = 1;
		rtSpec.m_colorFormat = m_dataFormat;
		rtSpec.m_numSamples = Gnm::kNumSamples1;
		rtSpec.m_numFragments = Gnm::kNumFragments1;
		if (m_isTile) {
			rtSpec.m_colorTileModeHint = Gnm::kTileModeDisplay_2dThin;
			m_tilingMode = SCE_VIDEO_OUT_TILING_MODE_TILE;
		}
		else {
			rtSpec.m_colorTileModeHint = Gnm::kTileModeDisplay_LinearAligned;
			m_tilingMode = SCE_VIDEO_OUT_TILING_MODE_LINEAR;
		}

		Gnm::RenderTarget rt;
		rt.init(&rtSpec);

		m_sizeAlign = rt.getColorSizeAlign();
		m_pitch = rt.getPitch();
		int arraySlice = 0;
		m_tilingParameters.initFromRenderTarget(&rt, arraySlice);
		if (m_isTile) {
			m_tiler2d.init(&m_tilingParameters);
		}
		else {
			m_tilerLinear.init(&m_tilingParameters);
		}
	};
	int getWidth() { return m_width; };
	int getHeight() { return m_height; };
	int getPitch() { return m_pitch; };
	int getSize() { return m_sizeAlign.m_size; };
	int getAlign() { return m_sizeAlign.m_align; };
	bool isTile() { return m_isTile; };
	void setDisplayBufferAttributes(SceVideoOutBufferAttribute *attribute) {
		sceVideoOutSetBufferAttribute(attribute,
			m_pixelFormat,
			m_tilingMode,
			SCE_VIDEO_OUT_ASPECT_RATIO_16_9,
			m_width,
			m_height,
			m_pitch);
	};
	Gnm::SizeAlign getSizeAlign(void) { return m_sizeAlign; };
	uint64_t getTiledByteOffset(uint32_t x, uint32_t y) {
		uint64_t tiledByteOffset;
		uint32_t z = 0;
		int ret = 0;
		int fragmentIndex = 0;
		if (m_isTile) {
			ret = m_tiler2d.getTiledElementByteOffset(&tiledByteOffset, x, y, z, fragmentIndex);
		}
		else {
			ret = m_tilerLinear.getTiledElementByteOffset(&tiledByteOffset, x, y, z);
		}
		assert((ret == GpuAddress::kStatusSuccess));
		return tiledByteOffset;
	};

private:
	int m_width;
	int m_pitch;
	int m_height;
	int m_pixelFormat;
	int m_bpp;
	bool m_isTile;
	int m_tilingMode;
	Gnm::DataFormat m_dataFormat;
	GpuAddress::TilingParameters m_tilingParameters;
	Gnm::SizeAlign m_sizeAlign;
	GpuAddress::Tiler2d m_tiler2d;
	GpuAddress::TilerLinear m_tilerLinear;
};

class RenderBuffer {
public:
	void init(RenderBufferAttributes *attr, void *address = NULL, int displayBufferIndex = -1) {
		m_attr = attr;
		m_address = address;
		m_displayBufferIndex = displayBufferIndex;
		m_flipArgLog = SCE_VIDEO_OUT_BUFFER_INITIAL_FLIP_ARG - 1;
		m_barPrevX = -1;
	}
	void setDisplayBufferIndex(int index) { m_displayBufferIndex = index; };
	int getDisplayBufferIndex(void) { return m_displayBufferIndex; };
	void *getAddress(void) { return m_address; };
	int getLastFlipArg(void) { return m_flipArgLog; };
	void setFlipArg(int64_t flipArg) { m_flipArgLog = flipArg; };
	RenderBufferAttributes *getRenderBufferAttributes(void) { return m_attr; };
	bool isTile(void) { return m_attr->isTile(); };
	int getWidth() { return m_attr->getWidth(); };
	int getHeight() { return m_attr->getHeight(); };
	int getPitch() { return m_attr->getPitch(); };
	int getBarPrevX(void) { return m_barPrevX; };
	void setBarPrevX(int x) { m_barPrevX = x; };
	uint64_t getTiledByteOffset(uint32_t x, uint32_t y) { return m_attr->getTiledByteOffset(x, y); };
private:
	RenderBufferAttributes *m_attr;
	void *m_address;
	int m_displayBufferIndex; // registered index by sceVideoOutRegisterBuffers
	int64_t m_flipArgLog; // flipArg of used last time when flipping to this buffer to track where in the flip queue this buffer currently is
	int m_barPrevX; // last positision of the green bar

};


/*
 *  simple memory allocation functions for video direct memory
 */
class VideoMemory {
public:
	void init(size_t size);
	void *memAlign(int size, int alignment);
	void reset(void);
	void finish(void);

private:
	uint64_t m_stackPointer;
	uint32_t m_stackSize;
	uint64_t m_stackMax;
	uint64_t m_stackBase;
	off_t m_baseOffset;
};
