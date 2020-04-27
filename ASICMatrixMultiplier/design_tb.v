module test;

    reg reset = 0;
    reg run = 0;
    initial begin
        #17 reset = 1;
        #10 run = 1;
        #20 run = 0;
        #3000 $finish;
    end

    reg clk = 0;
    always #5 clk = ~clk;

    wire [11:0] Xaddress;
    wire [11:0] Gaddress;
    wire [11:0] Waddress;
    wire [3:0] Row;
    wire [3:0] VecSelect;
    wire LoadVectors;
    wire Busy;

    reg [15:0] Multiplicand = 16'b0100000000000000;
    reg [15:0] Multiplier   = 16'b0100100000000000;//16'b1001000000000000;
    wire [19:0] Out;
    wire [15:0] TanhResult;
    wire [11:0] TanhAddress;
    wire [15:0] TanhData  = 16'b010000000000000;
    wire [3:0] TanhControl;
    wire RA;
    wire WriteEnable;
    wire Saturated;
    wire RomAddrSelect;

    Controller c1(clk, reset, run, Busy, Xaddress, Gaddress, Waddress, VecSelect, RA, TanhAvailable, WriteEnable, Saturated, RomAddrSelect);
    Matrix     m1(clk, reset, Multiplier, Multiplicand, VecSelect, Out);
    Tanh       t1(clk, reset, Out, TanhAddress, TanhData, RA, TanhAvailable, TanhResult, Saturated, RomAddrSelect);

    initial
        $monitor("Xaddress = %h, Gaddress = %h, TanhAddress = %h, Saturated = %h, Out = %h, RA = %h, WriteEnable = %h, TanhResult = %h, RomAddrSelect = %h",
            Xaddress, Gaddress, TanhAddress, Saturated, Out, RA, WriteEnable, TanhResult, RomAddrSelect);

endmodule