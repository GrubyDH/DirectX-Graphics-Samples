//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#pragma once

#include "DXSample.h"

using namespace DirectX;

// Note that while ComPtr is used to manage the lifetime of resources on the CPU,
// it has no understanding of the lifetime of resources on the GPU. Apps must account
// for the GPU lifetime of resources to avoid destroying objects that may still be
// referenced by the GPU.
// An example of this can be found in the class method: OnDestroy().
using Microsoft::WRL::ComPtr;

class D3D12HelloTexture : public DXSample
{
public:
	D3D12HelloTexture(UINT width, UINT height, std::wstring name);

	virtual void OnInit();
	virtual void OnUpdate();
	virtual void OnRender();
	virtual void OnDestroy();

private:
	static const UINT FrameCount = 2;
	static const UINT TextureWidth = 256;
	static const UINT TextureHeight = 256;
	static const UINT TexturePixelSize = 4;	// The number of bytes used to represent a pixel in the texture.

	struct Vertex
	{
		XMFLOAT3 position;
		XMFLOAT2 uv;
	};

	// Pipeline objects.
	CD3DX12_VIEWPORT m_viewport;
	CD3DX12_RECT m_scissorRect;
	ComPtr<IDXGISwapChain3> m_swapChain;
	ComPtr<ID3D12Device> m_device;
	ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
	ComPtr<ID3D12CommandAllocator> m_commandAllocator;
	ComPtr<ID3D12CommandQueue> m_commandQueue;
	ComPtr<ID3D12RootSignature> m_rootSignature;
	ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
	ComPtr<ID3D12DescriptorHeap> m_srvHeap;
	ComPtr<ID3D12PipelineState> m_pipelineState;
	ComPtr<ID3D12GraphicsCommandList> m_commandList;
	UINT m_rtvDescriptorSize;

	// App resources.
	ComPtr<ID3D12Resource> m_vertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
	ComPtr<ID3D12Resource> m_texture;

	// Synchronization objects.
	UINT m_frameIndex;
	HANDLE m_fenceEvent;
	ComPtr<ID3D12Fence> m_fence;
	UINT64 m_fenceValue;

    /* Initialize graphics pipeline:
       - Create device (warp or hardware, debug or not)
       - Comman queue (direct)
       - Swap chain (# FrameCount backbuffers)
       - RTV descriptor heap (# FrameCount descriptors)
       - SRV descriptor heap
       - Frame resources (RTs and RTVs)
       - Command allocator (direct)
    */
	void LoadPipeline();

    /* Initialize assets:
       - Root signature
       - VS, PS, PSO
       - Command list(direct)
       - Vertex buffer + view
       - Texture (upload and default resource)
       - SRV
       - Execute command list (actual texture creation)
       - Fence object
       - Fence event
       - Flush
    */
	void LoadAssets();
	std::vector<UINT8> GenerateTextureData();

    /* Populate command list:
       - Reset the command allocator
       - Reset the command queue (needs a command allocator and a PSO)
       - Set root signature
       - Set descriptor heap (SRV)
       - Set descriptor table in root signature
       - Set viewport and scissors
       - Transition backbuffer from PRESENT to RT
       - Clear RT
       - Set up input assembler
       - Draw
       - Transition backbuffer from RT to PRESENT
       - Close command list
    */
	void PopulateCommandList();
	void WaitForPreviousFrame();
};
