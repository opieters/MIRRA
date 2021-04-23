$fn=100;

// all in mm
// thickness of the outer wall
d_wall = 1.5;

// outer radius of cone (incl. wall)
r_outer = 70;

// inner diameter of cone (incl. wall)
r_inner = 45;

r_opening = (r_inner+r_outer)/2;

// total height of the cap
h_cap = 35;

// distance between two caps (used for height)
cap_sep = 0.5*h_cap; // separation distance (as fraction of h_cap)

// total number of caps (incl. top and bottom cap, must be at least 2)
n_caps = 6;

// width of profile (tangent to inner circle)
p_w = 16;

// depth of profile (perpendicular to inner circle)
p_d = 5;

// width of opening in the profile (parallel to inner circle tangent)
p_o_w = 12;

// depth of opening in the profile
p_o_d = 3;

// width of slot in profile
p_o_s_w = 8;

// for more info on nut dimensions:
// https://en.wikipedia.org/wiki/Nut_(hardware)#Standard_metric_hex_nuts_sizes

// nut to attach internal and external shield
// height of the nut
nut_h = 3.2;

// inner radius of the nut
nut_r = 4/2;

// outer radius of the nut
nut_re = 8.1/2;

// margin used around to make sure nut fits
nut_margin = 0.1;

// nut to connect boards
// height of the nut
nut_bc_h = 2.4;

// inner radius of the nut
nut_bc_r = 3.0/2;

// outer radius of the nut
nut_bc_re = 6.4/2;

// margin used around to make sure nut fits
nut_bc_margin = 0.05;

// lock depth (height of upper part and)
lock_h = 2;
lock_d = 3;
lock_margin = 0.1;

p_margin = 0.5;
p_o_wm = p_o_w - p_margin;
p_o_dm = p_o_d - p_margin;

// for the additional support of beams in inner part
bs_margin = 3;
bs_margin_h = 3;

ps_margin = 5; //
ps_margin_h = 5;

// hook parameters

// radius of the two holes to mount
hook_opening_r = 2.5;

// length of one side of the hook (full length)
hook_length = 80;

// width of the hook
hook_width = 18;
// thickness
hook_thickness = 2;

// location of first hole, measured from the outer edge (away from right angle)
hole1_center_d = 16;

// location of second hole, measured from the outer edge (away from right angle)
hole2_center_d = 62.5;