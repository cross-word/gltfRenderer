#include "stdafx.h"
#include "D3DCamera.h"

XMMATRIX const D3DCamera::GetViewMatrix()
{
    XMVECTOR pos = XMLoadFloat3(&m_position);
    XMMATRIX rot = XMMatrixRotationRollPitchYaw(m_pitch, m_yaw, 0.0f);
    XMVECTOR forward = XMVector3TransformNormal(XMVectorSet(0.f, 0.f, 1.f, 0.f), rot);
    XMVECTOR up = XMVector3TransformNormal(XMVectorSet(0.f, 1.f, 0.f, 0.f), rot);
    return XMMatrixLookAtLH(pos, pos + forward, up);
}

XMMATRIX const D3DCamera::GetProjectionMatrix(float aspect)
{
    return XMMatrixPerspectiveFovLH(XM_PIDIV4, aspect, 0.1f, 100.0f);
}

void D3DCamera::Move(const XMFLOAT3& delta)
{
    XMVECTOR d = XMLoadFloat3(&delta);
    XMMATRIX rot = XMMatrixRotationRollPitchYaw(m_pitch, m_yaw, 0.0f);
    d = XMVector3TransformNormal(d, rot);
    XMVECTOR pos = XMLoadFloat3(&m_position);
    pos += d;
    XMStoreFloat3(&m_position, pos);
}

void D3DCamera::Rotate(float dx, float dy)
{
    m_yaw += dx;
    m_pitch += dy;
    const float limit = XM_PIDIV2 - 0.01f;
    if (m_pitch > limit) m_pitch = limit;
    if (m_pitch < -limit) m_pitch = -limit;
}