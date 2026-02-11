use <threadlib/threadlib.scad>

$fn=120;
nutWidth=2.5/cos(180/6);
so_height=14;
thickness=5;
base_width=46.9;
base_height=71;
cutout_width=28;
cutout_height=10;
offset=2.849;
base_overlap=8;

carriage();

module carriage() {
    difference() {
        base();
        
        translate([-cutout_width/2,base_height-cutout_height,-1])
            cube([cutout_width,cutout_height+1, thickness+2]);

        translate([-18.85,-offset,-1])
            scale([1.1,1.1,1]) tap("M2", turns=12);
        translate([18.85,-offset,-1])
            scale([1.1,1.1,1]) tap("M2", turns=12);
        translate([-18.95,65.8-offset,-1])
            scale([1.1,1.1,1]) tap("M2", turns=12);
        translate([18.95,65.8-offset,-1])
            scale([1.1,1.1,1]) tap("M2", turns=12);
    }
}

module base() {
    translate([-18.85,0,0]) standoff();
    translate([18.85,0,0]) standoff();
    translate([-18.95,65.8,0]) standoff();
    translate([18.95,65.8,0]) standoff();
    
    translate([-base_width/2,-base_overlap-(base_height-65.8)/2,0]) cube([base_width,base_height+base_overlap,thickness]);
}

module standoff() {
    difference() {
        cylinder(so_height,nutWidth,nutWidth,$fn=6);
        translate ([0,0,so_height-5])
            scale([1.1,1.1,1]) tap("M2", turns=20);
    }
}