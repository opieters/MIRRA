include <parameters.scad>

include <radiation_shield_lib.scad>


bs_points = [
  [0,0,0], [p_o_w+2*bs_margin, 0,0], [p_o_w+2*bs_margin, p_o_d+2*bs_margin, 0], [0, p_o_d+2*bs_margin, 0], 
  [bs_margin,bs_margin,bs_margin_h], [p_o_w+bs_margin, bs_margin,bs_margin_h], [p_o_w+bs_margin, p_o_d+2*bs_margin, bs_margin_h], [bs_margin, p_o_d+2*bs_margin, bs_margin_h]];



ps_points = [
  [0,0,0], [p_w+2*ps_margin, 0,0], [p_w+2*ps_margin, p_d+ps_margin, 0], [0, p_d+ps_margin, 0], 
  [ps_margin,ps_margin,ps_margin], [p_w+ps_margin, ps_margin,ps_margin], [p_w+ps_margin, p_d+ps_margin, ps_margin], [ps_margin, p_d+ps_margin, ps_margin]];

bs_faces = [
  [0, 1, 2, 3], // bottom
  [7, 6, 5, 4], // top
  [0, 4, 5, 1], // front
  [1, 5, 6, 2], // right
  [3, 7, 4, 0], // left
  [2, 6, 7, 3], // back
];

ps_faces = bs_faces;



difference() {
  union() {
buttom_cap();



// draw support
for(i=[0:90:360]){
  rotate(v=[0,0,1], a=i) translate([-p_w/2,-r_inner+d_wall/2,-lock_d]) profile2(h=2*cap_sep + lock_d, lock_buttom=false);
}
  
//difference() {
// extra support for beams
for(i=[0:90:360]){
  rotate(v=[0,0,1], a=i) translate([0,r_inner-3*d_wall/2,h_cap]) color("black") difference() {
rotate(a=180, v=[0,0,1]) rotate(a=180, v=[0,1,0]) translate([-ps_margin-p_w/2,-ps_margin/2-p_d/2,0]) polyhedron(points = ps_points, faces = ps_faces, connectivity = 3);
translate([(-p_w-p_o_w)/4,(-p_d-p_o_d),-1.5*ps_margin_h]) cube([(p_w+p_o_w)/2, (p_d+p_o_d), 3*ps_margin_h]);
}
}
//color("red") translate([0,0,-0.5*h_cap]) cylinder(r1=r_inner-d_wall, r2=r_inner-d_wall, h=2*h_cap);
//}

  }

for(i=[0:90:360]){
  //rotate(v=[0,0,1], a=i) translate([-p_w/2,-r_inner+d_wall/2,0]) profile();
  rotate(v=[0,0,1], a=i) translate([-p_o_w/2, -p_o_d/2+r_inner+d_wall/2-p_d/2,h_cap]) translate([-bs_margin, -bs_margin, 0]) color("purple") translate([bs_margin+p_o_w/2,bs_margin/2+p_o_d/2,0]) rotate(a=180, v=[0,0,1]) rotate(a=180, v=[0,1,0]) translate([-bs_margin-p_o_w/2,-bs_margin/2-p_o_d/2,0]) polyhedron(points=bs_points, faces = bs_faces, connectivity = 3);;

}





}
