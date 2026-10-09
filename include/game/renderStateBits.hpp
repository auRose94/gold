#pragma once

// gold-native render state bit layouts.
//
// The draw-state / stencil / sampler parsers (graphics.cpp's setState,
// parseStencil, sampleStringToFlags) produce these bits; every render
// backend translates them to its API's own layout. Nothing in gold's
// facades names another engine's flag macros.

#include <cstdint>

namespace gold {

	/** Draw-state flag words and fields. Bit layout (gold's own):
	 *
	 *  bit 0-4    write masks R,G,B,A,Z
	 *  bit 5-7    depth compare (0=always 1=less 2=lequal 3=equal
	 *             4=gequal 5=greater 6=notequal 7=never)
	 *  bit 9-10   cull (0=none 1=cw 2=ccw)
	 *  bit 11     blend enabled
	 *  bit 12-14  blend equation (0=add 1=sub 2=revsub 3=min 4=max)
	 *  bit 15-18  rgb source factor
	 *  bit 19-22  rgb destination factor
	 *  bit 23-26  alpha source factor
	 *  bit 27-30  alpha destination factor
	 *  bit 31     independent blend
	 *  bit 32     alpha-to-coverage
	 *  bit 33-40  alpha reference value (0..255)
	 *  bit 41-43  primitive (0=triangle list 1=triangle strip
	 *             2=line list 3=line strip 4=points)
	 *  bit 44     MSAA
	 *  bit 45     line AA
	 *  bit 46     conservative raster
	 */
	enum drawStateBits : uint64_t {
		WriteR = 1ull << 0,
		WriteG = 1ull << 1,
		WriteB = 1ull << 2,
		WriteA = 1ull << 3,
		WriteZ = 1ull << 4,
		DepthCompareShift = 5,
		CullShift = 9,
		BlendEnabled = 1ull << 11,
		BlendEquationShift = 12,
		BlendRGBSrcShift = 15,
		BlendRGBDstShift = 19,
		BlendASrcShift = 23,
		BlendADstShift = 27,
		BlendIndependent = 1ull << 31,
		BlendAlphaToCoverage = 1ull << 32,
		AlphaRefShift = 33,
		PrimitiveShift = 41,
		StateMSAA = 1ull << 44,
		StateLineAA = 1ull << 45,
		StateConservativeRaster = 1ull << 46,
	};

	/** Depth comparators (the DepthCompare field). */
	enum stateDepthCompare : uint64_t {
		DepthAlways = 0, DepthLess, DepthLEqual, DepthEqual,
		DepthGEqual, DepthGreater, DepthNotEqual, DepthNever,
	};

	/** Cull face mode (the Cull field): none, clockwise, or
	 *  counter-clockwise front. */
	enum stateCull : uint64_t {
		CullNone = 0, CullCW = 1, CullCCW = 2,
	};

	/** Blend equations (the BlendEquation field). */
	enum stateBlendEquation : uint64_t {
		BlendAdd = 0, BlendSub, BlendRevSub, BlendMin, BlendMax,
	};

	/** Blend factors (the four factor fields, in enum order = the bit
	 *  values packed into the fields above). */
	enum stateBlendFactor : uint64_t {
		FactorZero = 0, FactorOne, FactorSrcColor, FactorInvSrcColor,
		FactorSrcAlpha, FactorInvSrcAlpha, FactorDstAlpha,
		FactorInvDstAlpha, FactorDstColor, FactorInvDstColor,
		FactorSrcAlphaSat, FactorBlendFactor, FactorInvBlendFactor,
	};

	/** Primitives (the Primitive field). */
	enum statePrimitive : uint64_t {
		PrimitiveTriangles = 0, PrimitiveTriStrip, PrimitiveLines,
		PrimitiveLineStrip, PrimitivePoints,
	};

	/** Stencil layout, per face (gold's own; the facade reports the
	 *  whole packed uint32 per side):
	 *
	 *  bit 0-2   test (stateDepthCompare values)
	 *  bit 3-5   stencil-fail op
	 *  bit 6-8   depth-fail op
	 *  bit 9-11  depth-pass op
	 *
	 *  Ops: 0=keep 1=zero 2=replace 3=incr 4=incr-sat 5=decr
	 *  6=decr-sat 7=invert
	 */
	enum stencilBits : uint32_t {
		StencilTestShift = 0,
		StencilFailShift = 3,
		StencilZFailShift = 6,
		StencilZPassShift = 9,
	};
	enum stencilOps : uint32_t {
		OpKeep = 0, OpZero, OpReplace, OpIncr, OpIncrSat, OpDecr,
		OpDecrSat, OpInvert,
	};

	/**
	 * Sampler flag words: three bits per texture axis (mirror, clamp,
	 * border), point-sampling and anisotropy selects, a mip point-sample,
	 * then a shadow-compare toggle with the mode in the depth-compare
	 * field's values.
	 */
	enum samplerBits : uint32_t {
		MirrorU = 1u << 0, ClampU = 1u << 1, BorderU = 1u << 2,
		MirrorV = 1u << 3, ClampV = 1u << 4, BorderV = 1u << 5,
		MirrorW = 1u << 6, ClampW = 1u << 7, BorderW = 1u << 8,
		MinPoint = 1u << 9, MinAnisotropic = 1u << 10,
		MagPoint = 1u << 11, MagAnisotropic = 1u << 12,
		MipPoint = 1u << 13,
		CompareEnabled = 1u << 14,
		CompareModeShift = 15,
	};

	/** Submission discard flags (bitwise). */
	enum submitDiscardBits : uint16_t {
		DiscardNone = 0,
		DiscardState = 1u << 0,
		DiscardTransform = 1u << 1,
		DiscardBindings = 1u << 2,
		DiscardVertexStreams = 1u << 3,
		DiscardIndexBuffer = 1u << 4,
		DiscardInstanceData = 1u << 5,
		DiscardAll = uint16_t((1u << 6) - 1),
	};

	/** Vertex/index buffer creation flags (gold-native; a backend
	 *  translates them to its own buffer bits). */
	enum bufferFlags : uint64_t {
		BufferNone = 0,
		/** Index entries are 32-bit. Absent, an index buffer is 16-bit —
		 *  the glTF UNSIGNED_INT vs UNSIGNED_SHORT split. */
		BufferIndex32 = 1ull << 0,
	};

}  // namespace gold