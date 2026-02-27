#ifndef VERTEX_H
#define VERTEX_H

#include <SimpleMath.h>

using namespace DirectX::SimpleMath;

struct Vertex {
    Vector3 position;
    Vector3 normal;
    Vector2 texCoord;
};

#endif // VERTEX_H