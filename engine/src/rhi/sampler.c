#include "rhi/sampler.h"
#include "core/handle_internal.h"
#include "rhi/instance.h"
#include "sampler_internal.h"

#ifdef RHI_BACKEND_DX12_AVAILABLE
#include "dx12/dx12.h"
#endif

DEFINE_HANDLE_POOL(RHI_Sampler, RHI_Sampler_Impl *);
static RHI_SamplerPool s_samplers;

RHI_Sampler RHI_Sampler_Create(RHI_Instance i, RHI_Device d,
                               const RHI_SamplerDesc *desc) {
	if (!desc || (unsigned)desc->minFilter > RHI_FILTER_LINEAR ||
			(unsigned)desc->magFilter > RHI_FILTER_LINEAR ||
			(unsigned)desc->mipFilter > RHI_FILTER_LINEAR ||
			(unsigned)desc->addressU > RHI_ADDRESS_MIRROR_CLAMP ||
			(unsigned)desc->addressV > RHI_ADDRESS_MIRROR_CLAMP ||
			(unsigned)desc->addressW > RHI_ADDRESS_MIRROR_CLAMP ||
			(unsigned)desc->compareOp > RHI_SAMPLER_COMPARE_ALWAYS ||
			(unsigned)desc->borderColor > RHI_BORDER_OPAQUE_WHITE ||
			desc->minLod > desc->maxLod ||
			(desc->anisotropyEnable &&
			(desc->maxAnisotropy < 2 || desc->maxAnisotropy > 16))) {
		return RHI_Sampler_Invalid();
	}
	RHI_Sampler h = RHI_SamplerPool_Alloc(&s_samplers);
	RHI_Sampler_Impl **p = RHI_SamplerPool_Resolve(&s_samplers, h);
	bool ok = false;
	switch (RHI_Instance_Get_Backend(i)) {
		case RHI_BACKEND_DX12:
	#ifdef RHI_BACKEND_DX12_AVAILABLE
			ok = DX12_Sampler_Create(d, desc, p);
	#endif
			break;
	default:
			break;
	}
	if (!ok) {
		RHI_SamplerPool_Free(&s_samplers, h);
		return RHI_Sampler_Invalid();
	}
	return h;
}
void RHI_Sampler_Destroy(RHI_Sampler h) {
	RHI_Sampler_Impl **p = RHI_SamplerPool_Resolve(&s_samplers, h);
	if (!p || !*p)
		return;
	(*p)->vtable->destroy(*p);
	*p = NULL;
	RHI_SamplerPool_Free(&s_samplers, h);
}
