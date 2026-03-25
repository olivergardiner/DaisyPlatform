use <threadlib/threadlib.scad>

$fn=120;
nutWidth=2.2/cos(180/6);
so_height=11;
thickness=3.6;
base_width=47;
base_height=71;
cutout_width=28;
cutout_height=10;
offset=0;
base_overlap=0;
border=62.;
disp_h=61.3;
disp_w=44;
fillet=6.35;

//hole();
bezel();
//carriage();
//bezel_shaper();

module bezel() {
    difference() {
        bezel_shaper();
        
        translate([border,border,0])
            cube([disp_w,disp_h,thickness]);
            
        translate([(disp_w-cutout_width)/2+border,0,0])
            cube([cutout_width,border,1.8]);
            
        translate([disp_w/2+border-18.95,border-2.2,thickness-1])
            hole();
        translate([disp_w/2+border+18.95,border-2.2,thickness-1])
            hole();
        translate([disp_w/2+border-18.95,disp_h+border+2.6,thickness-1])
            hole();
        translate([disp_w/2+border+18.95,disp_h+border+2.6,thickness-1])
            hole();
    }
}

module bezel_shaper() {
    hull() {
        translate([fillet/2,fillet/2,0])
            cylinder(d=fillet,h=thickness);
        translate([disp_w+2*border-fillet/2,fillet/2,0])
            cylinder(d=fillet,h=thickness);
        translate([fillet/2,disp_h+2*border-fillet/2,0])
            cylinder(d=fillet,h=thickness);
        translate([disp_w+2*border-fillet/2,disp_h+2*border-fillet/2,0])
            cylinder(d=fillet,h=thickness);
    }
}

module hole() {
    rotate([180,0,0])
        scale([1.3,1.3,1.1]) tap("M2", turns=12);
}

module base() {
    translate([-18.85,0,0]) standoff();
    translate([18.85,0,0]) standoff();
    translate([-18.95,65.8,0]) standoff();
    translate([18.95,65.8,0]) standoff();
    bezel();
}

module standoff() {
    difference() {
        cylinder(so_height,nutWidth,nutWidth,$fn=6);
        //translate ([0,0,so_height-5])
            //scale([1.1,1.1,1]) tap("M2", turns=20);
    }
}