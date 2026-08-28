#version 140

uniform sampler2D sampler;
uniform sampler2D previousFrame;
uniform vec2 cursorPosition;
uniform vec2 cursorRadius;
uniform int time;
//float damp;
//float phase;
uniform vec2 screenSize;

in vec2 texcoord0;

out vec4 fragColor;

void main()
{
    //vec4 currentFrame = texture(sampler, texcoord0);
    //fragColor = mix(currentFrame, texture(previousFrame, texcoord0), 0.1);

    float damp = 0.93;
    float phase = 0.45;

    vec2 texelSize = 1.0 / screenSize;

    vec4 curr_rgb = texture(sampler, texcoord0);
    vec4 prev_rgb = texture(previousFrame, texcoord0);

    float new_r = (2.0 * prev_rgb.r - prev_rgb.g + phase * (
                    texture(previousFrame, texcoord0 - vec2(0.0, texelSize.y)).r +
                    texture(previousFrame, texcoord0 + vec2(0.0, texelSize.y)).r +
                    texture(previousFrame, texcoord0 - vec2(texelSize.x, 0.0)).r +
                    texture(previousFrame, texcoord0 + vec2(texelSize.x, 0.0)).r - 4.0 * prev_rgb.r)) * damp;

   /* if (curr_rgb.r > 0.4 && prev_rgb.b <= 0.4) {
        new_r += curr_rgb.r * 0.95;
    }

    if (prev_rgb.b > 0.1 && curr_rgb.r <= 0.1) {
        new_r -= prev_rgb.b * 0.5;
    }
*/
    //new_r = clamp(new_r, 0.0, 0.4)
    vec4 result = vec4(new_r, prev_rgb.r, curr_rgb.b, 1.0);

    float timeFactor = 1.0 - abs(min(1.0, (float(time) / 200.0)) - 0.5) * 2.0;
    float circleDistance = length((texcoord0 - cursorPosition) / (cursorRadius * timeFactor));
    float circle = 1.0 - smoothstep(0.8, 1.0, circleDistance);

    fragColor = mix(result, vec4(1.0, 0.0, 0.0, 1.0), circle);
}
