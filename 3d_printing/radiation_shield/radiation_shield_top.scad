$fn=100;

// all in mm
d_wall = 1.5;
r_outer = 60;
r_inner = 35;
h_cap = 35;
cap_sep = 0.5*h_cap; // separation distance (as fraction of h_cap)
n_caps = 4;
p_w = 8;
p_d = 5;
p_o_w = 5;
p_o_d = 2;
p_o_s_w = 2;

nut_h = 4;
nut_r = 2.5;
nut_re = 9.2/2;
nut_margin = 0.1;

// define modules
module cap() {
  difference() {
  cylinder(r=r_inner, r2=r_outer, h=h_cap);
  t = (r_outer - d_wall - r_inner + d_wall) / h_cap;
  h = 5;
  r_i = r_inner - t * h - d_wall;
  r_o = t * h + r_outer - d_wall;
  translate([0,0,-h]) cylinder(r1=r_i, r2= r_o, h=h_cap+2*h);
  }
}

module profile(h) {
  //translate([p_w/2,0,0])
  difference() {
    cube([p_w, p_d, h]);
    union() {
      translate([(p_w-p_o_w)/2,(p_d-p_o_d)/2,-0.25*h]) cube([p_o_w, p_o_d, 1.5*h]);
      translate([(p_w-p_o_s_w)/2,p_d-1.5*(p_d-p_o_d)/2,-0.25*h]) cube([p_o_s_w, 2*p_o_d, 1.5*h]);
      color("blue") translate([-p_w/2,p_d/2,h-2]) cube([2*p_w, p_d, 4]);
    }
  }
  
}

module top_cap() {
  t = h_cap / (r_outer - r_inner);
  dr = (h_cap + d_wall) / t;
  ro = r_inner - d_wall + dr;
  difference() {
    cylinder(r1=r_inner, r2=r_outer, h=h_cap);
    translate([0,0,d_wall]) cylinder(r1=r_inner-d_wall, r2=ro, h=h_cap+d_wall);
  }
  //translate([0,0,-h_cap/6]) cylinder(r1=0, r2=r_inner, h=h_cap/6);
  for(i=[0:90:360]){
    rotate(v=[0,0,1], a=i) translate([-p_w/2,-r_inner+d_wall/2,0]) profile(h=cap_sep);
  }
}

module buttom_cap() {
  t = h_cap / (r_outer - d_wall - r_inner + d_wall);
  r_o = r_outer - d_wall;
  difference() {
    union() {
      difference(){
        difference() {
          cylinder(r1=r_inner, r2=r_outer, h=h_cap);
          translate([0,0,-t*r_o + h_cap - d_wall]) cylinder(r1=0, r2= r_o, h=t*r_o);
        }
        color("red") translate([0,0,h_cap-2*d_wall]) cylinder(r1=r_inner-d_wall, r2=r_inner, h=8*d_wall);
      }
      for(i = [0:90:360-45]) {
        rotate(a=i, v=[0,0,1]) translate([r_inner, -1.5*nut_re, h_cap-nut_h-d_wall]) nut_support();
      }
    }
    union() {
      for(i = [0:90:360-45]) {
        rotate(a=i, v=[0,0,1]) translate([r_inner+1.5*nut_re, 0, h_cap - nut_h]) scale([1,1,2]) nut(h=nut_h, rh=nut_r, re=nut_re, margin=nut_margin);
        rotate(a=i, v=[0,0,1]) translate([r_inner+1.5*nut_re, 0, h_cap]) cylinder(r=nut_r + nut_margin, h=3*nut_h, center=true);
      }
    }
  }
}


module nut(h, rh, re, margin) {
// h height of nut
// r radius of screw wire
// w width of nut between two horizontal sides

r = rh + margin;
ro = re + margin;
order = 6;
angles=[ for (i = [0:order-1]) i*(360/order) ];
   coords=[ for (th=angles) [ro*cos(th), ro*sin(th)] ];
   linear_extrude(height=h)
   difference(){
   
  polygon(coords);
  circle(r=r);
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
h = nut_h + d_wall;
t = h_cap / (r_outer - r_inner);
dr = h / t;
ri = r_outer - dr;
translate([-r_inner,1.5*nut_re,0])
difference(){
  intersection() {
    cylinder(r1=ri, r2=r_outer, h=h);
    //translate([0,0,-t*r_o + h_cap - d_wall]) cylinder(r1=0, r2= r_o, h=t*r_o);
    translate([0,-1.5*nut_re,-nut_h/2]) cube([2*r_outer, 3*nut_re, 2*h]);
  }
  cube([2*r_inner, 5*nut_re, 6*nut_h], center=true);
  //color("red") translate([0,0,h_cap-2*d_wall]) cylinder(r1=r_inner-d_wall, r2=r_inner, h=8*d_wall);
}
}


// draw actual shape

top_cap();



