include <parameters.scad>
include <radiation_shield_lib.scad>

$fn=200;

// entire shape
difference(){
union () {
  // flat top surface
  difference(){
    cylinder(r1=r_inner, r2=r_inner+d_wall, h=d_wall);

    translate([7.5,0,0])
    cylinder(r=4, h=4*d_wall,center=true);

    translate([0.3, 29.5, 0])
    cylinder(r=4, h=4*d_wall,center=true);
  }

  translate([0,0,d_wall])
  difference(){
    cylinder(r=13.5,h=2);
    cylinder(r=12.5,h=15,center=true);
  }

  // draw each cap
  for( i = [0:n_caps-1]){
    color("red")
    translate([0,0,i*cap_sep]) cap(r_inner, r_outer, h_cap, d_wall);
  }

  // flat bottom surface 
  difference() 
  {
    h_slope = 7*d_wall;
    // draw filled shape
    {
      t = h_cap / (r_outer - r_inner - d_wall);
      r_o = r_outer;
      dr = (h_slope) / t;
      ri = r_outer - dr;
      color("red") translate([0,0,(n_caps-1)*cap_sep+h_cap-h_slope]) cylinder(r1=ri, r2=r_outer, h=h_slope);
    }
    // create skewed inner part
    color("blue") translate([0,0,(n_caps-1)*cap_sep+h_cap-h_slope+2*d_wall]) cylinder(r1=ri, r2=r_opening, h=h_slope, center=true);

    // hole in the centre
    translate([0,0,(n_caps-1)*cap_sep+h_cap-2*d_wall]) cylinder(r=r_opening, h=5*d_wall, center=true);
  }

  // create support for nuts to attach inner and outer parts
  translate([0,0,(n_caps-1)*cap_sep]) 
  difference(){
    // nut support
    for(i = [0:90:360-45]) {
      rotate(a=i, v=[0,0,1]) translate([r_inner, -1.5*nut_re, h_cap-nut_h-3*nut_h]) nut_support();
    }

    // nuts and holes for nuts
    union() {
      for(i = [0:90:360-45]) {
        rotate(a=i, v=[0,0,1]) 
        translate([r_inner+1.5*nut_re, 0, h_cap - nut_h]) 
        scale([1,1,2]) 
        nut(h=nut_h, rh=nut_r, re=nut_re, margin=-3*nut_margin, fill=true);
        
        rotate(a=i, v=[0,0,1]) 
        translate([r_inner+1.5*nut_re, 0, 10*nut_h]) 
        cylinder(r=nut_r + nut_margin, h=10*nut_h, center=true);
      }
    }
  }

  difference()
  {

    union(){
      // draw support
      for(i=[0:90:90]){
        color("blue")
        rotate(v=[0,0,1], a=i) 
        translate([-p_w/2,-r_inner,0]) 
        profile(h=(n_caps-1)*cap_sep+h_cap);
      }

      difference(){
        cylinder(r=r_inner+d_wall, h=(n_caps-1)*cap_sep+h_cap);

        translate([0,0,-d_wall]) 
        cylinder(r=r_inner, h=(n_caps-1)*cap_sep+h_cap+2*d_wall);

        for(i = [27.5:90:360]){
          for(j = [1:n_caps]){
            rotate(a=i, v=[0,0,1]) 
            scale([1.5,1,1]) 
            translate([0,0,j*cap_sep-cap_sep/2])
            translate([0,((n_caps-1)*cap_sep+h_cap)/2,0]) 
            rotate(a=90, v=[1,0,0]) 
            cylinder(r=0.4*cap_sep, h=(n_caps-1)*cap_sep+h_cap);
          }
          {
          j = n_caps+1;
            rotate(a=i, v=[0,0,1]) 
            //scale([1.5,1,1]) 
            translate([0,0,j*cap_sep-cap_sep/2])
            translate([0,((n_caps-1)*cap_sep+h_cap)/2,0]) 
            rotate(a=90, v=[1,0,0]) 
            cylinder(r=0.3*cap_sep, h=(n_caps-1)*cap_sep+h_cap);
          }
        }

        for(i = [-27.5:90:360]){
          for(j = [1:n_caps]){
            rotate(a=i, v=[0,0,1]) 
            scale([1.5,1,1]) 
            translate([0,0,j*cap_sep-cap_sep/2])
            translate([0,((n_caps-1)*cap_sep+h_cap)/2,0]) 
            rotate(a=90, v=[1,0,0]) 
            cylinder(r=0.4*cap_sep, h=(n_caps-1)*cap_sep+h_cap);
          }
          {j = n_caps+1;
            rotate(a=i, v=[0,0,1]) 
            //scale([1.5,1,1]) 
            translate([0,0,j*cap_sep-cap_sep/2])
            translate([0,((n_caps-1)*cap_sep+h_cap)/2,0]) 
            rotate(a=90, v=[1,0,0]) 
            cylinder(r=0.3*cap_sep, h=(n_caps-1)*cap_sep+h_cap);
          }
        }


        for(i = [45:90:360]){
            rotate(a=i, v=[0,0,1]) 
            //scale([2,1,1]) 
            translate([0,0,(n_caps+1)*cap_sep])
            translate([0,((n_caps-1)*cap_sep+h_cap)/2,0]) 
            rotate(a=90, v=[1,0,0]) 
            cylinder(r=0.4*cap_sep, h=(n_caps-1)*cap_sep+h_cap);
        }


      }
    }

    
    bs_points = [
      // buttom plane
      [-bs_margin,-bs_margin,-bs_margin_h], 
      [p_o_w+3*bs_margin, -bs_margin,-bs_margin_h], 
      [p_o_w+3*bs_margin, p_o_d+3*bs_margin, -bs_margin_h], 
      [-bs_margin, p_o_d+3*bs_margin, -bs_margin_h], 
      // top plane
      [1.2*bs_margin,1.2*bs_margin,1.2*bs_margin_h], 
      [p_o_w+0.8*bs_margin, 1.2*bs_margin,1.2*bs_margin_h], 
      [p_o_w+0.8*bs_margin, p_o_d+0.8*bs_margin, 1.2*bs_margin_h], 
      [1.2*bs_margin, p_o_d+0.8*bs_margin, 1.2*bs_margin_h]
    ];

    bs_points2 = [
      // buttom plane
      [0,0,0], 
      [p_o_w+2*bs_margin, 0,0], 
      [p_o_w+2*bs_margin, p_o_d+2*bs_margin, 0], 
      [0, p_o_d+2*bs_margin, 0], 
      // top plane
      [bs_margin,bs_margin,bs_margin_h], 
      [p_o_w+bs_margin, bs_margin,bs_margin_h], 
      [p_o_w+bs_margin, p_o_d+bs_margin, bs_margin_h], 
      [bs_margin, p_o_d+bs_margin, bs_margin_h]
    ];

    bs_faces = [
      [0, 1, 2, 3], // bottom
      [7, 6, 5, 4], // top
      [0, 4, 5, 1], // front
      [1, 5, 6, 2], // right
      [3, 7, 4, 0], // left
      [2, 6, 7, 3], // back
    ];

for(i=[180:90:360]){
      union(){
        rotate(v=[0,0,1], a=i) 
        translate([-p_o_w/2, -p_o_d/2+r_inner+d_wall/2-p_d/2,(n_caps-1)*cap_sep+h_cap]) 
        translate([-bs_margin, -bs_margin, 0]) 
        translate([bs_margin+p_o_w/2,bs_margin/2+p_o_d/2,0]) 
        rotate(a=180, v=[0,0,1]) 
        rotate(a=180, v=[0,1,0])
        translate([-bs_margin-p_o_w/2,-bs_margin/2-p_o_d/2,0]) 
        polyhedron(points=bs_points, faces = bs_faces, connectivity = 3);
      }
    }
    

    
  }

}
  profile_opening_width = 2;
  profile_opening_depth = 2;

  for(i=[0:90:90]){
    rotate(a=i, v=[0,0,1])
    translate([0,-r_inner+(p_d-p_o_d)/2,0]) // move to first position
    translate([-1.25*profile_opening_width/2,-2*profile_opening_depth/2,d_wall]) // move to centre position
    cube([1.25*profile_opening_width, 2*profile_opening_depth, 1.5*((n_caps-1)*cap_sep+h_cap)]);
  }

  //translate([0,0,-5])
  //cube([500,500,500]);
}