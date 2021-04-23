include <parameters.scad>
include <radiation_shield_lib.scad>


$fn = 100;

ps_h = 3*nut_bc_h+d_wall; // must be larger than d_wall+nut_bc_h!!
d_board = nut_bc_re;
pw_w = 2*nut_bc_re+(p_d-p_o_d)+d_board;
ps_d = p_o_s_w-1;
ps_overlap = 0.5;
n_pcb = 0;
h_pcb = 1.6;

level_sep = 25;


bs_points = [
  [0,0,0], [p_o_wm+2*bs_margin, 0,0], [p_o_wm+2*bs_margin, p_o_dm+2*bs_margin, 0], [0, p_o_dm+2*bs_margin, 0], 
  [bs_margin,bs_margin,bs_margin_h], [p_o_wm+bs_margin, bs_margin,bs_margin_h], [p_o_wm+bs_margin, p_o_dm+bs_margin, bs_margin_h], [bs_margin, p_o_dm+bs_margin, bs_margin_h]];

bs_faces = [
  [0, 1, 2, 3], // bottom
  [7, 6, 5, 4], // top
  [0, 4, 5, 1], // front
  [1, 5, 6, 2], // right
  [3, 7, 4, 0], // left
  [2, 6, 7, 3], // back
];

r_bolts_pcb = r_inner-p_d+(p_d-p_o_d)/2 + p_margin/2 - d_board - nut_bc_re;
r_pcb_max = r_inner - p_d;

echo("The PCB board should have bold openings in a radius of ", r_bolts_pcb, "mm");
echo("The PCB board should have a max radius of ", r_pcb_max, "mm");


//translate([0,0,-d_wall/2])
//union(){

/*for(i = [0:n_pcb-1]){
  color("green")
  translate([0,0,10+ps_h+level_sep*i])
  difference(){
    cylinder(r=r_pcb_max, h=h_pcb);
    for(i = [0:90:360]){
      rotate(v=[0,0,1], a=i)
      translate([0,r_bolts_pcb,0])
      cylinder(r=nut_bc_r, h=4*h_pcb, center=true);
    }
  }
}*/




r_close = 0.8*r_outer + 0.2*r_inner;

// define modules

// draw actual shape


b_l = 2*cap_sep + n_pcb*cap_sep; // beam length
xsp = 0;
difference(){
  union(){
intersection(){
for(shift = [-6, 39]){
  translate([shift,0, 0])
  difference(){
    cylinder(r1=6, r2=4, h=5);

    translate([0,0,5-3])
    nut(h=4, rh=1,re=2.3, margin=0, fill=true);

    translate([0,0,5-1])
    nut(h=4, rh=1,re=3, margin=0, fill=true);
  }
}
cylinder(r=r_inner, h=10, center=true);
}

rotate(a=45,v=[0,0,1])
union(){

// draw support + beams
for(i=[0:90:90]){
  rotate(v=[0,0,1], a=i) 
  translate([0,-r_inner+p_d/2,0]) 
  translate([-p_o_wm/2,-p_o_dm/2,0])
  union() {
    color("red") 
    cube([p_o_wm, p_o_dm ,b_l]);


    translate([-bs_margin, 0, 0]) 
    color("purple") 
    translate(op) rotate(a=180, v=[0,0,1]) 
    translate(-op) 
    polyhedron(points=bs_points, faces = bs_faces, connectivity = 3);
  }

  //rotate(v=[0,0,1], a=i) translate([-p_o_s_w/2, -p_o_dm/2+r_inner-d_wall/2-p_d/2-xsp,0]) color("black") cube([p_o_s_w, p_o_dm/2+xsp ,b_l]);
}

op = [p_o_wm+2*bs_margin, p_o_dm+bs_margin, 0]/2;

difference() {
  cylinder(r=r_close, h=d_wall, center=true);

  union() {
    for(i = [15:30:360]) {
      rotate(a=i, v=[0,0,1]) translate([r_inner+1.5*nut_re, 0, 0]) cylinder(r=1.5*nut_bc_r + nut_bc_margin, h=3*nut_bc_h, center=true);
    }
    for(i = [0:90:360]) {
      rotate(a=i, v=[0,0,1]) translate([r_inner+1.5*nut_re, 0, 0]) cylinder(r=1.5*nut_bc_r + nut_bc_margin, h=3*nut_bc_h, center=true);
    }
  }
}







/*
include <helix_extrude.scad>

helix_extrude(angle=360*(n_caps), height=b_l) {
  translate([ps_d/2-r_inner+d_wall/2+p_d/2+xsp, 0, 0]) {
    square([d_wall,0.3*cap_sep]);
  }
}*/

/*helix_extrude(angle=360*(n_caps), height=b_l) {
  translate(-[ps_d/2-r_inner+d_wall/2+p_d/2+xsp, 0, 0]) {
    square([d_wall,0.3*cap_sep]);
  }
}*/
/*
translate([0,0,b_l])
scale([1,1,0.5])
intersection() {
  //difference(){
sphere(r=abs(ps_d-r_inner+d_wall+p_d+xsp));
//sphere(r=r_inner-5/2*d_wall);
  //}
linear_extrude([0,0,r_outer]) square([2*r_outer, 2*r_outer], center=true);
}*/
//}


//translate([0,0,10+level_sep+ps_h]) cylinder(r=40, h=2);


  profile_opening_width = 1.8;
  profile_opening_depth = 2;

  for(i=[0:90:90]){
    rotate(a=i, v=[0,0,1])
    translate([0,-r_inner+p_d/2,0]) 
    translate([0,p_o_dm/2,0])
    translate([-1.25*profile_opening_width/2,-2*profile_opening_depth,0*d_wall]) // move to centre position
    cube([1.25*profile_opening_width, 2*profile_opening_depth, b_l]);
  }
}
  }
  translate([16.5,0,0])
  cylinder(r=15, h=10, center=true);

  translate([-10,27.5,0])
  cylinder(r=15, h=10, center=true);

  translate([-10,-27.5,0])
  cylinder(r=15, h=10, center=true);

  translate([-30,0,0])
  cylinder(r=12.5, h=10, center=true);

  translate([15,25,0])
  cylinder(r=5, h=10, center=true);

  translate([15,-25,0])
  cylinder(r=5, h=10, center=true);

  for(shift = [-6, 39]){
    translate([shift,0, 0])
    cylinder(r=3.2/2, h=15, center=true);
  }
}