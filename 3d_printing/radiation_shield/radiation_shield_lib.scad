// define modules
module cap(ri, ro, h_cap, d_wall) {
  difference() {
  cylinder(r=ri+d_wall, r2=ro, h=h_cap);
  t = (ro - d_wall - ri) / h_cap;
  h = 5;
  r_i = ri - t * h;
  r_o = t * h + ro - d_wall;
  translate([0,0,-h]) cylinder(r1=r_i, r2= r_o, h=h_cap+2*h);
  }
}

module profile(h) {
  //translate([p_w/2,0,0])
  difference() {
    cube([p_w, p_d, h]);
    union() {
      translate([(p_w-p_o_w)/2,(p_d-p_o_d)/2,-0.25*h])
      cube([p_o_w, p_o_d, 1.5*h]);
      
      translate([(p_w-p_o_s_w)/2,p_d-1.5*(p_d-p_o_d)/2,-0.25*h]) 
      cube([p_o_s_w, 2*p_o_d, 1.5*h]);

    }
  }
}

module profile2(h, lock_buttom=true) {
    difference() {
        profile(h=h);
        if(lock_buttom){
      color("blue") translate([-p_w/2,-2*lock_d+p_d/2-lock_d/2,h-lock_h]) cube([2*p_w, 2*lock_d+2*lock_margin, 2*lock_h]);
      color("blue") translate([-p_w/2,p_d-(p_d/2-lock_d/2)-lock_margin,h-lock_h]) cube([2*p_w, 2*lock_d+4*lock_margin, 2*lock_h]);
        }
        color("green") translate([-p_w/2,(p_d-lock_d)/2,-lock_h]) cube([2*p_w, lock_d, 2*lock_h]);
        //color("green") translate([-p_w/2,-lock_d+0.3*d_wall,-1.8*lock_h]) cube([2*p_w, lock_d, 2*lock_h]);
    }
      

}


module top_cap() {

  difference() {
    cylinder(r1=r_inner, r2=r_outer, h=h_cap);
    translate([0,0,d_wall]) cylinder(r1=r_inner-d_wall, r2=r_outer-d_wall, h=h_cap-d_wall);
  }
  //translate([0,0,-h_cap/6]) cylinder(r1=0, r2=r_inner, h=h_cap/6);

}

module buttom_cap() {
  t = h_cap / (r_outer - r_inner);
  r_o = r_outer - d_wall;
  dr = (h_cap + d_wall) / t;
  ri = r_outer - d_wall - dr;
  difference() {
    union() {
      difference(){
        difference() {
          cylinder(r1=r_inner, r2=r_outer, h=h_cap);
          translate([0,0,-d_wall]) color("white") cylinder(r1=ri, r2= r_o, h=h_cap);
        }
        color("red") translate([0,0,h_cap-2*d_wall]) cylinder(r1=r_inner-d_wall, r2=r_inner-d_wall, h=8*d_wall);
      }
      for(i = [45:90:360-45]) {
        rotate(a=i, v=[0,0,1]) translate([r_inner, -1.5*nut_re, h_cap-nut_h-d_wall]) nut_support();
      }
    }
    union() {
      for(i = [45:90:360-45]) {
        rotate(a=i, v=[0,0,1]) translate([r_inner+1.5*nut_re, 0, h_cap - nut_h]) scale([1,1,2]) nut(h=nut_h, rh=nut_r, re=nut_re, margin=nut_margin);
        rotate(a=i, v=[0,0,1]) translate([r_inner+1.5*nut_re, 0, h_cap]) cylinder(r=nut_r + nut_margin, h=3*nut_h, center=true);
      }
    }
  }
}


module nut(h, rh, re, margin, fill=false) {
// h height of nut
// r radius of screw wire
// w width of nut between two horizontal sides

r = rh + margin;
ro = re + margin;
order = 6;
angles=[ for (i = [0:order-1]) i*(360/order) ];
   coords=[ for (th=angles) [ro*cos(th), ro*sin(th)] ];
   if(fill){
    linear_extrude(height=h) polygon(coords);
   } else {
    linear_extrude(height=h)
    difference(){
      polygon(coords);
      circle(r=r);
    }
   }
}

module nut2(h, r, w, margin) {
// h height of nut
// r radius of screw wire
// w width of nut between two horizontal sides

s = w/(2 * sin(30));
x = w/(2 * tan(30));
ro = w/2+x + margin;
order = 6;
angles=[ for (i = [0:order-1]) i*(360/order) ];
   coords=[ for (th=angles) [ro*cos(th), ro*sin(th)] ];
   linear_extrude(height=h)
   difference(){
   
  polygon(coords);
  circle(r=r);
   }
}

module nut_support() {
  h = nut_h + 3*nut_h;
  t = h_cap / (r_outer - r_inner);
  dr = h / t;
  ri = r_outer - dr;
  dx = 2*(r_outer - r_opening); //(r_outer - r_inner);


  sc_points = [
    [0,0,0], [dx-dr, 0,0], [dx-dr, h, 0], [0, h, 0], 
    [0,0,h-nut_h], [dx-dr, 0,0], [dx-dr, h, ], [0, h, h-nut_h]];

  ps_faces = [
    [0, 1, 2, 3], // bottom
    [7, 6, 5, 4], // top
    [0, 4, 5, 1], // front
    [1, 5, 6, 2], // right
    [3, 7, 4, 0], // left
    [2, 6, 7, 3], // back
  ];

  points = [[0,0], [0,h-1.5*nut_h], [dx-dr, 0]];



  difference()
  {

    translate([-r_outer+dx,1.5*nut_re,0])
    difference()
    {
      intersection() {
        cylinder(r1=ri, r2=r_outer, h=h);
        //translate([0,0,-t*r_o + h_cap - d_wall]) cylinder(r1=0, r2= r_o, h=t*r_o);
        translate([0,-1.5*nut_re,-nut_h/2]) 
        cube([2*r_outer, 3*nut_re, 2*h]);
      }
      cube([2*(r_outer - dx), dx, 3*h], center=true);
      
      //color("red") translate([0,0,h_cap-2*d_wall]) cylinder(r1=r_inner-d_wall, r2=r_inner, h=8*d_wall);
    }

    translate(-[dx-dr,3.5*nut_re,h-3*d_wall]/2) 
    scale([2,2,2])
    translate([0, 3.5*nut_re, 0]) 
    rotate(a=90, v=[1,0,0]) 
    color("black") 
    linear_extrude(height=4*nut_re) 
    polygon(points=points);
    
    //translate([0,-2.5*nut_re/2,0]) color("white") polyhedron(points = sc_points, faces = ps_faces, connectivity = 3);
  }
}



