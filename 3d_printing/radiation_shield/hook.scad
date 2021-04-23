include <parameters.scad>
include <radiation_shield_lib.scad>

module hook(){
    difference(){
        translate([0,-hook_width/2,0])
        union(){
            cube([hook_length, hook_width, hook_thickness]);

            translate([hook_thickness,0,0]) 
            rotate(v=[0,1,0], a=-90) 
            cube([hook_length, hook_width, hook_thickness]);
        }

        translate([hook_length-hole1_center_d,0,-2*hook_thickness]) 
        cylinder(r=hook_opening_r, h=5*hook_thickness);

        translate([hook_length-hole2_center_d,0,-2*hook_thickness]) 
        cylinder(r=hook_opening_r, h=5*hook_thickness);

        rotate(v=[0,1,0], a=-90) 
        translate([hook_length-hole1_center_d,0,-2*hook_thickness]) 
        cylinder(r=hook_opening_r, h=5*hook_thickness);

        rotate(v=[0,1,0], a=-90) 
        translate([hook_length-hole2_center_d,0,-2*hook_thickness]) 
        cylinder(r=hook_opening_r, h=5*hook_thickness);
    }
}