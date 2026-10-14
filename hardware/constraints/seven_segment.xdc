## Basys 3 4-digit 7-segment display
## GPIO_0_tri_o[0:6] = CA..CG
## GPIO_0_tri_o[7]   = DP
## GPIO_0_tri_o[8:11] = AN0..AN3

## Segment A
set_property PACKAGE_PIN W7 [get_ports {GPIO_0_tri_o[0]}]
set_property IOSTANDARD LVCMOS33 [get_ports {GPIO_0_tri_o[0]}]

## Segment B
set_property PACKAGE_PIN W6 [get_ports {GPIO_0_tri_o[1]}]
set_property IOSTANDARD LVCMOS33 [get_ports {GPIO_0_tri_o[1]}]

## Segment C
set_property PACKAGE_PIN U8 [get_ports {GPIO_0_tri_o[2]}]
set_property IOSTANDARD LVCMOS33 [get_ports {GPIO_0_tri_o[2]}]

## Segment D
set_property PACKAGE_PIN V8 [get_ports {GPIO_0_tri_o[3]}]
set_property IOSTANDARD LVCMOS33 [get_ports {GPIO_0_tri_o[3]}]

## Segment E
set_property PACKAGE_PIN U5 [get_ports {GPIO_0_tri_o[4]}]
set_property IOSTANDARD LVCMOS33 [get_ports {GPIO_0_tri_o[4]}]

## Segment F
set_property PACKAGE_PIN V5 [get_ports {GPIO_0_tri_o[5]}]
set_property IOSTANDARD LVCMOS33 [get_ports {GPIO_0_tri_o[5]}]

## Segment G
set_property PACKAGE_PIN U7 [get_ports {GPIO_0_tri_o[6]}]
set_property IOSTANDARD LVCMOS33 [get_ports {GPIO_0_tri_o[6]}]

## Decimal point
set_property PACKAGE_PIN V7 [get_ports {GPIO_0_tri_o[7]}]
set_property IOSTANDARD LVCMOS33 [get_ports {GPIO_0_tri_o[7]}]

## Digit select AN0
set_property PACKAGE_PIN U2 [get_ports {GPIO_0_tri_o[8]}]
set_property IOSTANDARD LVCMOS33 [get_ports {GPIO_0_tri_o[8]}]

## Digit select AN1
set_property PACKAGE_PIN U4 [get_ports {GPIO_0_tri_o[9]}]
set_property IOSTANDARD LVCMOS33 [get_ports {GPIO_0_tri_o[9]}]

## Digit select AN2
set_property PACKAGE_PIN V4 [get_ports {GPIO_0_tri_o[10]}]
set_property IOSTANDARD LVCMOS33 [get_ports {GPIO_0_tri_o[10]}]

## Digit select AN3
set_property PACKAGE_PIN W4 [get_ports {GPIO_0_tri_o[11]}]
set_property IOSTANDARD LVCMOS33 [get_ports {GPIO_0_tri_o[11]}]