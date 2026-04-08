float getTerrainHeight(float x, float z)
{
    // simple noise or function
    return sin(x * 0.1f) * 2.0f + cos(z * 0.1f) * 2.0f;
}