#version 450 core

layout (triangles) in;
layout (triangle_strip) out;
layout (max_vertices = 3) out;

layout(location = 0) in vertexData
{
    flat int sliceId;
} vertices[];

void main()
{
    for (int i = 0; i < 3; ++i)
    {
        gl_Position = gl_in[i].gl_Position;
        gl_Layer = vertices[0].sliceId;
        EmitVertex();
    }
    EndPrimitive();
}