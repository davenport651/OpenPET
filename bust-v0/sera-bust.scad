// =====================================================================
//  Chibi Sera bust for the Sera-vice (LOLIN S3 Mini Pro) — v0 draft
//  Marcus: tweak the numbers at the top, everything follows.
//  Units: mm. Front of the bust faces +Y. Up is +Z.
//  Print: bust body (part="body") and face frame (part="frame").
// =====================================================================

// ---------- Board (from LOLIN docs; screen/button spots = caliper-check) ----------
BOARD_W = 34.3;      // X
BOARD_H = 25.4;      // Z  (landscape, exactly as the firmware draws)
CLEAR   = 0.6;       // pocket clearance per side
SCREEN_WIN = 20;     // square window for the 0.85" TFT glass
SCREEN_DY  = 1.5;    // screen centre above board centre (CALIPER-CHECK!)
BTN_D    = 5.5;      // button holes in the frame
BTN_DX   = 9.0;      // button spacing from centre (CALIPER-CHECK!)
BTN_DZ   = -11.0;     // buttons below screen centre (CALIPER-CHECK!)

// ---------- Bust proportions ----------
HEAD_R   = 40;       // head sphere radius (low-poly faceted)
HEAD_Z   = 82;       // head centre height
WALL     = 3;        // shell wall thickness

part = "body";       // "body" or "frame"

// rounded box helper (flat in Y)
module rbox(w,h,d,r){
  hull(){ for(x=[-1,1], z=[-1,1])
    translate([x*(w/2-r), 0, z*(h/2-r)]) rotate([90,0,0]) cylinder(h=d, r=r, center=true, $fn=24);
  }
}

// ---------- solid bust ----------
module bust_solid(){
  color([0.45,0.45,0.5]) cylinder(h=4, r=40, $fn=12);
  color([0.55,0.55,0.6]) translate([0,0,4]) cylinder(h=5, r=35, $fn=12);
  color([0.16,0.55,0.36]) translate([0,0,9]) cylinder(h=24, r1=27, r2=19, $fn=12);
  color([0.16,0.55,0.36]) translate([0,0,33]) scale([1,0.92,0.55]) sphere(r=21, $fn=12);
  color([0.95,0.76,0.6]) translate([0,0,38]) cylinder(h=14, r=11, $fn=10);
  color([0.95,0.76,0.6]) translate([0,0,HEAD_Z]) scale([1,0.94,1.02]) sphere(r=HEAD_R, $fn=14);
  color([0.35,0.2,0.1]) translate([0,-4,HEAD_Z+5]) scale([1.03,1.0,1.05]) sphere(r=HEAD_R, $fn=14);
  for(x=[-1,1]) color([0.35,0.2,0.1])
    translate([x*33,-2,HEAD_Z-16]) scale([0.55,0.6,1.25]) sphere(r=13, $fn=10);
  color([0.1,0.65,0.4]) translate([0,0,HEAD_Z+16]) rotate([72,0,0]) scale([1,1,0.55])
    rotate_extrude(angle=360, $fn=28) translate([36.5,0,0]) circle(r=3.6, $fn=12);
}

// ---------- subtractions ----------
module bust_cuts(){
  translate([0, 27, HEAD_Z+2]) rbox(BOARD_W+2*CLEAR, BOARD_H+2*CLEAR, 16, 3);
  translate([0, 30, HEAD_Z+2]) rbox(BOARD_W+8, BOARD_H+10, 22, 4);
  translate([0,-2,HEAD_Z+2]) scale([1,0.94,1.02]) sphere(r=HEAD_R-WALL-1, $fn=14);
  translate([-7, -1, 8]) cube([14, 11, 44]);
  translate([0,-38,7]) rotate([90,0,0]) cylinder(h=10, r=5, $fn=16);
}

module body_part(){ difference(){ bust_solid(); bust_cuts(); } }

// ---------- face frame (snaps over the pocket) ----------
module frame_part(){
  color([0.3,0.3,0.35]) difference(){
    translate([0, 33.5, HEAD_Z+2]) rbox(BOARD_W+9, BOARD_H+9, 3.2, 5);
    translate([0, 33.5, HEAD_Z+2+SCREEN_DY]) cube([SCREEN_WIN, 6, SCREEN_WIN], center=true);
    for(x=[-BTN_DX, 0, BTN_DX])
      translate([x, 33.5, HEAD_Z+2+BTN_DZ]) rotate([90,0,0]) cylinder(h=6, r=BTN_D/2, center=true, $fn=16);
  }
}

if (part == "body") body_part(); else if (part == "frame") frame_part(); else { body_part(); frame_part(); }
