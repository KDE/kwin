float sdfRoundedBox(vec2 position, vec2 center, vec2 extents, vec4 radius) {
    vec2 p = position - center;
    float r = p.x > 0.0
        ? (p.y < 0.0 ? radius.y : radius.w)
        : (p.y < 0.0 ? radius.x : radius.z);
    vec2 q = abs(p) - extents + vec2(r);
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;
}
