.section .rodata

.global  shader_vs
.global  shader_fs

.global  texture_ground
.global  texture_ground_size
.global  texture_light_0
.global  texture_light_0_size
.global  texture_light_1
.global  texture_light_1_size
.global  texture_light_2
.global  texture_light_2_size

shader_vs:
    .incbin "resources/shader.vs"
    .byte 0
shader_fs:
    .incbin "resources/shader.fs"
    .byte 0


texture_ground:
    .incbin "resources/circuit_board.jpg"
texture_ground_size:
    .int texture_ground_size - texture_ground;
texture_light_0:
    .incbin "resources/light_0.png"
texture_light_0_size:
    .int texture_light_0_size - texture_light_0
texture_light_1:
    .incbin "resources/light_1.png"
texture_light_1_size:
    .int texture_light_1_size - texture_light_1
texture_light_2:
    .incbin "resources/light_2.png"
texture_light_2_size:
    .int texture_light_2_size - texture_light_2
