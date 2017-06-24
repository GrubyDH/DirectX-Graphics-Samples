#pragma once

#include "Model.h"

namespace Graphics {

class MeshGenerator {
public:

static void Sphere(Model& model, Vector3 pos, float radius, uint16_t nSlices, uint16_t nStacks) {
    // Generate vertex/index data.
    std::vector<Vertex> vertices;
    std::vector<DirectX::XMFLOAT3> verticesDepth;
    std::vector<uint16_t> indices;

    // Poles: note that there will be texture coordinate distortion as there is
    // not a unique point on the texture map to assign to the pole when mapping
    // a rectangular texture onto a sphere.
    Vertex topVertex(
        pos.GetX(), pos.GetY() + radius, pos.GetZ(),
        0.0f, 0.0f,
        0.0f, +1.0f, 0.0f,
        1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f
    );
    Vertex bottomVertex(
        pos.GetX(), pos.GetY() - radius, pos.GetZ(),
        0.0f, 1.0f,
        0.0f, -1.0f, 0.0f,
        1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, -1.0f);

    vertices.push_back(topVertex);
    float phiStep = DirectX::XM_PI / nStacks;
    float thetaStep = 2.0f * DirectX::XM_PI / nSlices;

    // Compute vertices for each stack ring (do not count the poles as rings).
    for (uint16_t i = 1; i <= nStacks - 1; ++i)
    {
        float phi = i * phiStep;

        // Vertices of ring.
        for (uint16_t j = 0; j <= nSlices; ++j)
        {
            float theta = j * thetaStep;

            Vertex v;

            // Spherical to cartesian.
            v.Position.x = pos.GetX() + radius * sinf(phi) * cosf(theta);
            v.Position.y = pos.GetY() + radius * cosf(phi);
            v.Position.z = pos.GetZ() + radius * sinf(phi) * sinf(theta);

            v.TexC.x = theta / XM_2PI;
            v.TexC.y = phi / XM_PI;

            XMVECTOR p = XMLoadFloat3(&v.Position);
            XMStoreFloat3(&v.Normal, XMVector3Normalize(p));

            // Partial derivative of P with respect to theta.
            v.TangentU.x = -radius * sinf(phi) * sinf(theta);
            v.TangentU.y = 0.0f;
            v.TangentU.z = +radius * sinf(phi) * cosf(theta);
            XMVECTOR T = XMLoadFloat3(&v.TangentU);
            XMStoreFloat3(&v.TangentU, XMVector3Normalize(T));

            // Partial derivatives of P with respect to phi.
            v.BitangentU.x = +radius * cosf(phi) * cosf(theta);
            v.BitangentU.y = -radius * sinf(phi);
            v.BitangentU.z = +radius * cosf(phi) * sinf(theta);
            XMVECTOR B = XMLoadFloat3(&v.BitangentU);
            XMStoreFloat3(&v.BitangentU, XMVector3Normalize(B));

            vertices.push_back(v);
        }
    }

    vertices.push_back(bottomVertex);

    verticesDepth.resize(vertices.size());
    for (uint32_t i = 0; i < verticesDepth.size(); ++i)
        verticesDepth[i] = vertices[i].Position;

    // Compute indices for top stack. The top stack was written first to the vertex buffer
    // and connects the top pole to the first ring.
    for (uint16_t i = 1; i <= nSlices; ++i)
    {
        indices.push_back(0);
        indices.push_back(i + 1);
        indices.push_back(i);
    }

    // Compute indices for inner stacks (not connected to poles).

    // Offset the indices to the index of the first vertex in the first ring.
    // This is just skipping the top pole vertex.
    uint16_t baseIndex = 1;
    uint16_t ringVertexCount = nSlices + 1;
    for (uint16_t i = 0; i < nStacks - 2; ++i)
    {
        for (uint16_t j = 0; j < nSlices; ++j)
        {
            indices.push_back(baseIndex + i*ringVertexCount + j);
            indices.push_back(baseIndex + i*ringVertexCount + j + 1);
            indices.push_back(baseIndex + (i + 1)*ringVertexCount + j);

            indices.push_back(baseIndex + (i + 1)*ringVertexCount + j);
            indices.push_back(baseIndex + i*ringVertexCount + j + 1);
            indices.push_back(baseIndex + (i + 1)*ringVertexCount + j + 1);
        }
    }

    // Compute indices for bottom stack.  The bottom stack was written last to the vertex buffer
    // and connects the bottom pole to the bottom ring.

    // South pole vertex was added last.
    uint16_t southPoleIndex = static_cast<uint16_t>(vertices.size() - 1);

    // Offset the indices to the index of the first vertex in the last ring.
    baseIndex = southPoleIndex - ringVertexCount;

    for (uint16_t i = 0; i < nSlices; ++i)
    {
        indices.push_back(southPoleIndex);
        indices.push_back(baseIndex + i);
        indices.push_back(baseIndex + i + 1);
    }

    CreateModel(model, vertices, verticesDepth, indices);
}

private:
    struct Vertex {
        Vertex() {}
        Vertex(
            float px, float py, float pz,
            float nx, float ny, float nz,
            float tx, float ty, float tz,
            float bx, float by, float bz,
            float u, float v) :
            Position(px, py, pz),
            Normal(nx, ny, nz),
            TangentU(tx, ty, tz),
            BitangentU(bx, by, bz),
            TexC(u, v) {}

        DirectX::XMFLOAT3 Position;
        DirectX::XMFLOAT2 TexC;
        DirectX::XMFLOAT3 Normal;
        DirectX::XMFLOAT3 TangentU;
        DirectX::XMFLOAT3 BitangentU;
    };

static void CreateModel(Model& model, const std::vector<Vertex>& vertices, const std::vector<DirectX::XMFLOAT3>& verticesDepth, const std::vector<uint16_t>& indices) {

        unsigned int vertexStride = sizeof(Vertex);
        unsigned int vertexStrideDepth = sizeof(DirectX::XMFLOAT3);
        uint32_t vertexDataByteSize = static_cast<uint32_t>(vertices.size() * vertexStride);
        uint32_t indexDataByteSize = static_cast<uint32_t>(indices.size() * sizeof(uint16_t));
        uint32_t vertexDataByteSizeDepth = static_cast<uint32_t>(verticesDepth.size() * vertexStrideDepth);

        model.m_Header = { 1, 1, vertexDataByteSize, indexDataByteSize, vertexDataByteSizeDepth };

        unsigned int attribsEnabled = Model::attrib_mask_position | Model::attrib_mask_texcoord0 | Model::attrib_mask_normal | Model::attrib_mask_tangent | Model::attrib_mask_bitangent;
        unsigned int attribsEnabledDepth = Model::attrib_mask_position;

        model.m_pMesh = new Model::Mesh[1]{ {
            Model::BoundingBox{},
            0,                                              // materialIndex
            attribsEnabled,
            attribsEnabledDepth,
            vertexStride,
            vertexStrideDepth,
            {                                               // attrib
                { 0, 0, 3, Model::attrib_format_float },
                { sizeof(DirectX::XMFLOAT3), 0, 2, Model::attrib_format_float },
                { sizeof(DirectX::XMFLOAT3) + sizeof(DirectX::XMFLOAT2), 0, 3, Model::attrib_format_float },
                { 2 * sizeof(DirectX::XMFLOAT3) + sizeof(DirectX::XMFLOAT2), 0, 3, Model::attrib_format_float },
                { 3 * sizeof(DirectX::XMFLOAT3) + sizeof(DirectX::XMFLOAT2), 0, 3, Model::attrib_format_float }
            },
            {                                               // attribDepth
                { 0, 0, 3, Model::attrib_format_float }
            },

            0,                                              // vertexDataByteOffset
            static_cast<unsigned int>(vertices.size()),     // vertexCount
            0,                                              // indexDataByteOffset
            static_cast<unsigned int>(indices.size()),      // indexCount

            0,                                              // vertexDataByteOffsetDepth
            static_cast<unsigned int>(verticesDepth.size()) // vertexCountDepth
        } };

        model.m_pMaterial = new Model::Material[1]{ {
            Vector3(0.6f, 0.6f, 0.6f),  // diffuse
            Vector3(0.0f, 0.0f, 0.0f),  // specular
            Vector3(0.0f, 0.0f, 0.0f),  // ambient
            Vector3(0.0f, 0.0f, 0.0f),  // emissive
            Vector3(0.0f, 0.0f, 0.0f),  // transparent
            1.0f,                       // opacity
            0.0f,                       // shininess
            1.0f                        // specularStrength
        } };

        model.m_VertexStride = vertexStride;
        model.m_VertexStrideDepth = vertexStrideDepth;

        model.m_VertexBuffer.Create(L"VertexBuffer", vertexDataByteSize / vertexStride, vertexStride, vertices.data());
        model.m_IndexBuffer.Create(L"IndexBuffer", indexDataByteSize / sizeof(uint16_t), sizeof(uint16_t), indices.data());

        model.m_VertexBufferDepth.Create(L"VertexBufferDepth", vertexDataByteSizeDepth / vertexStrideDepth, vertexStrideDepth, verticesDepth.data());
        model.m_IndexBufferDepth.Create(L"IndexBufferDepth", indexDataByteSize / sizeof(uint16_t), sizeof(uint16_t), indices.data());

        // Required for calculating bounding box.
        model.m_pVertexData = const_cast<unsigned char*>(reinterpret_cast<const unsigned char*>(vertices.data()));
        model.ComputeAllBoundingBoxes();
        model.m_pVertexData = nullptr;

        model.LoadTextures();
    }
};

};
