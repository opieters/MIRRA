include <parameters.scad>
include <radiation_shield_lib.scad>

r_anchor = (0.6*r_outer + 0.4*r_inner);
r_attach = 0.8*r_inner;
r_hole = 17.5;
r_hole2 = 50;
a_margin = 5;
o_anchor = 10;

d_support = r_attach - a_margin;

module anchor_disk(connector){
    difference(){
        union(){
        difference(){
            cylinder(r=r_anchor, h=d_wall);

            union(){
                translate([0,0,-d_wall])
                cylinder(r=r_hole, h=3*d_wall);

                intersection() {
                    for(i = [180]){
                        rotate(a=i, v=[0,0,1])
                        cube([2*r_anchor, 2*o_anchor, 4*d_wall], center=true);
                    }

                    translate([0,0,-d_wall])
                    cylinder(r=r_hole2, h=3*d_wall);
                }
            }
        }

        for(i = [45, 135, 90]){
            rotate(v=[0,0,1], a=i)
            translate([0,0,d_wall/2])
            cube([2*r_attach,2*nut_re,d_wall], center=true);
        }

        for(i = [45:90:360]){
            rotate(v=[0,0,1], a=i)
            translate([r_attach, 0, 0]) 
            difference()
            {
                bs_points = [
                    // buttom plane
                    [0,0,0], 
                    [2*nut_re+2*d_wall+2*a_margin, 0,0], 
                    [2*nut_re+2*d_wall+2*a_margin, 2*nut_re+2*d_wall+2*a_margin, 0], 
                    [0, 2*nut_re+2*d_wall+2*a_margin, 0], 
                    // top plane
                    [a_margin,a_margin,nut_h+d_wall], 
                    [2*nut_re+2*d_wall+a_margin, a_margin,nut_h+d_wall], 
                    [2*nut_re+2*d_wall+a_margin, 2*nut_re+2*d_wall+a_margin, nut_h+d_wall], 
                    [a_margin, 2*nut_re+2*d_wall+a_margin, nut_h+d_wall]
                ];

                bs_faces = [
                    [0, 1, 2, 3], // bottom
                    [7, 6, 5, 4], // top
                    [0, 4, 5, 1], // front
                    [1, 5, 6, 2], // right
                    [3, 7, 4, 0], // left
                    [2, 6, 7, 3], // back
                ];

                translate(-[2*nut_re+2*d_wall+2*a_margin, 2*nut_re+2*d_wall+2*a_margin, 0]/2)
                polyhedron(points=bs_points, faces = bs_faces, connectivity = 3);

                union(){
                    translate([0,0,d_wall])
                    if(connector == "nut"){
                        nut(h=2*nut_h, rh=nut_r, re=nut_re, margin=nut_margin, fill=true);
                    } else {
                        cylinder(h=2*nut_h, r=nut_re+nut_margin, margin=nut_margin);
                    }
                }
            }
        }
        }
        for(i = [45:90:360]){
            rotate(v=[0,0,1], a=i)
            translate([r_attach, 0, 0])
            cylinder(r=nut_r, h=5*nut_h, center=true);
        }
    }
}

anchor_disk(connector="nut");

translate([0,1.5*r_anchor+d_wall, 1.5*r_anchor])
rotate(v=[1,0,0], a=90)
anchor_disk(connector="screw");

for(i = [-r_attach, 0, r_attach]){
    translate([i, 0, 0])
    rotate(v=[0,0,1], a=90)
    rotate(v=[1,0,0], a=90)
    
    difference(){
        translate([0,0,-d_wall])
        linear_extrude(height=2*d_wall)
        polygon([[-d_support,0], [d_support, 0], [d_support+r_anchor, r_anchor], [d_support+r_anchor, r_anchor+2*d_support]]);
        for(j = [0, 1, 2, 3]){
            translate([8*nut_re*j, 8*nut_re*j, 0])
            //translate([nut_re,0,0])
            cylinder(r=4*nut_re, h=5*d_wall, center=true);
        }
    }
}

bs_points = [
    // buttom plane
    [d_wall+a_margin, -d_support-d_wall-a_margin,0], 
    [-d_wall-a_margin, -d_support-d_wall-a_margin, 0], 
    [-d_wall-a_margin, d_support+d_wall+a_margin, 0], 
    [d_wall+a_margin, d_support+d_wall+a_margin, 0], 
    // top plane
    [0, -d_support, a_margin+d_wall], 
    [0, d_support, a_margin+d_wall], 
];

bs_faces = [
    [0, 1, 2, 3], // bottom
    [0, 4, 5, 1], // front
    [1, 5, 2], // right
    [3, 4, 0], // left
    [2, 5, 4, 3], // back
];


    intersection()
    {
    cylinder(r=r_anchor, h=10*a_margin, center=true);
for(i = [-r_attach, 0, r_attach]){

    translate([i, 0, 0])
    translate([0,a_margin+d_wall, 0])
    polyhedron(points=bs_points, faces = bs_faces, connectivity = 3);
    }
}

translate([0,1.5*r_anchor+d_wall, 1.5*r_anchor])
rotate(v=[1,0,0], a=90)
for(i = [-r_attach, 0, r_attach]){
    translate([i, 0, 0])
    translate(-[0,a_margin, 0])
    polyhedron(points=bs_points, faces = bs_faces, connectivity = 3);
}