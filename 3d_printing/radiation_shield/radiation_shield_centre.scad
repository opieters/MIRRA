include <parameters.scad>

include <radiation_shield_lib.scad>


cap(r_inner, r_outer, h_cap, d_wall);

// draw support
for(i=[0:90:360]){
  rotate(v=[0,0,1], a=i) translate([-p_w/2,-r_inner+d_wall/2,0]) profile2(h=cap_sep);
}


