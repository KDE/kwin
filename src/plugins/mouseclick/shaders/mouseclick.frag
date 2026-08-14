#version 140

uniform sampler2D sampler;
uniform sampler2D feedbackTexture;
uniform vec4 cursorColor;
//uniform vec2 cursorPosition;
//uniform vec2 cursorRadius;

in vec2 texcoord0;

out vec4 fragColor;

void main()
{
    vec4 screen = texture(sampler, texcoord0);
    vec4 feedback = texture(feedbackTexture, texcoord0);
   // float circleDistance = length((texcoord0 - cursorPosition) / cursorRadius);
   // float circle = 1.0 - smoothstep(0.8, 1.0, circleDistance);
   // fragColor = mix(screen, feedback, feedback.a);
    //fragColor = mix(fragColor, vec4(1.0, 0.0, 0.0, 1.0), circle);

    float factor = 1.0 - (feedback.r - feedback.g);// * 0.2;
    factor = clamp(factor, 0.9, 1.1);
    vec2 scaledCoord = texcoord0 * factor + vec2(0.5 - factor * 0.5);
    vec4 tex = texture(sampler, scaledCoord);
    tex = mix(tex, cursorColor, min(feedback.r, max(0.4, cursorColor.a)));

    fragColor = tex;
}
