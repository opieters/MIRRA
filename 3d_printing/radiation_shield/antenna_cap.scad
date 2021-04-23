$fn = 100;

//rotate(v=[1,0,0], a=90)
intersection()
{
difference()
{
    union(){
        translate([0,-6, 0]) 
        cube([8,4,70],center=true);

        cylinder(r=7, h=70, center=true);

        translate([0,0,-70/2])    
        difference(){
        
        //cylinder(r1=12, r2=2, h=7.5);

        //translate([0,0,-0.5])
        //cylinder(r1=5, r2=1.25, h=5);
        }

    }

    translate([0,0,-2.5])
    cylinder(r=5.5, h=70, center=true);


}

//translate([0,5-2,0])
cube([20,20,100],center=true);

//cube([14,14,100],center=true);
}