float lerp(float a, float b, float c) {
    return a + c * (b - a);
}

float defaultPerlinInterp(float t) {
    return t * t * t * (t * (t * 6.0F - 15.0F) + 10.0F);
}
