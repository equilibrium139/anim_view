#pragma once

struct Input
{
    float mouse_x;
    float mouse_y;
    float mouse_delta_x;
    float mouse_delta_y;

    int window_width, window_height;

    bool w_pressed, a_pressed, s_pressed, d_pressed;
    bool left_mouse_pressed;
};