#pragma once
#include <cstdint>

struct UnityEngine_Vector2
{
    float x;
    float y;
};

enum class UnityEngine_TouchPhase
{
    Began = 0,
    Moved = 1,
    Stationary = 2,
    Ended = 3,
    Canceled = 4,
};

enum class UnityEngine_TouchType
{
    Direct = 0,
    Indirect = 1,
    Stylus = 2,
};

struct UnityEngine_Touch
{
    int32_t m_FingerId;
    UnityEngine_Vector2 m_Position;
    UnityEngine_Vector2 m_RawPosition;
    UnityEngine_Vector2 m_PositionDelta;
    float m_TimeDelta;
    int32_t m_TapCount;
    UnityEngine_TouchPhase m_Phase;
    UnityEngine_TouchType m_Type;
    float m_Pressure;
    float m_maximumPossiblePressure;
    float m_Radius;
    float m_RadiusVariance;
    float m_AltitudeAngle;
    float m_AzimuthAngle;
};

namespace Unity
{
    void HookInput();
}
