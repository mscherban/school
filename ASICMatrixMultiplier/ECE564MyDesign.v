//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
// DUT




module ECE564MyDesign
	    (

            //---------------------------------------------------------------------------
            // Control
            //
            input  wire               xxx__dut__run            ,             
            output wire                dut__xxx__busy           , // high when computing
            input  wire               clk                      ,
            input  wire               reset_b                  ,


            //--------------------------------------------------------------------------- 
            //---------------------------------------------------------------------------
            // SRAM interface
            //
            output wire  [11:0]        dut__sram__write_address  ,
            output wire  [15:0]        dut__sram__write_data     ,
            output wire                dut__sram__write_enable   ,
            output wire  [11:0]        dut__sram__read_address   ,
            input  wire [15:0]        sram__dut__read_data      ,
            
            //---------------------------------------------------------------------------
            // f memory interface
            //
            output reg  [11:0]        dut__fmem__read_address   ,
            input  wire [15:0]        fmem__dut__read_data      ,  // read data

            //---------------------------------------------------------------------------
            // g memory interface
            //
            output wire  [11:0]        dut__gmem__read_address   ,
            input  wire [15:0]        gmem__dut__read_data      ,  // read data

            //---------------------------------------------------------------------------
            // i memory interface
            //
            output reg  [11:0]        dut__imem__read_address   ,
            input  wire [15:0]        imem__dut__read_data      ,  // read data
            
            //---------------------------------------------------------------------------
            // o memory interface
            //
            output reg  [11:0]        dut__omem__read_address   ,
            input  wire [15:0]        omem__dut__read_data      ,  // read data

            //---------------------------------------------------------------------------
            // Tanh look up table tanhmem 
            //
            output wire  [11:0]        dut__tanhmem__read_address     ,
            input  wire [15:0]        tanhmem__dut__read_data           // read data

            );

  //---------------------------------------------------------------------------
  //
  //<<<<----  YOUR CODE HERE    ---->>>>


    wire [3:0] VecSelect;
    wire MatrixAvailable;
    wire TanhAvailable;
    wire Saturated;
    wire TanhRomAddrSel;
    wire [19:0] Out;

//module Controller(clk, reset, run, Busy, Xaddress, Gaddress, Waddress,
 //               VecSelect, MatrixAvailable,
//                TanhAvailable, WriteEnable, Saturated, TanhRomAddrSel);


    Controller C1(clk, reset_b,
        xxx__dut__run,
        dut__xxx__busy,
        dut__sram__read_address,
        dut__gmem__read_address,
        dut__sram__write_address,
        VecSelect,
        MatrixAvailable,
        TanhAvailable,
        dut__sram__write_enable,
        Saturated, TanhRomAddrSel);


//module Matrix(clk, reset, Xdata, Gdata, VecSelect, Out);
    Matrix M1(clk, reset_b, sram__dut__read_data, gmem__dut__read_data, VecSelect, Out);


//module Tanh(clk, reset, Data, TanhAddress, TanhData, WorkAvailable, TanhAvailable, TanhResult, Saturated, RomAddrSelect);
    Tanh T1(clk, reset_b, Out, dut__tanhmem__read_address,
    tanhmem__dut__read_data, MatrixAvailable, TanhAvailable, dut__sram__write_data, Saturated, TanhRomAddrSel);
    
endmodule

module Tanh(clk, reset, Data, TanhAddress, TanhData, WorkAvailable, TanhAvailable, TanhResult, Saturated, RomAddrSelect);
    input clk;
    input reset;
    input WorkAvailable;
    input [19:0] Data;
    output TanhAvailable;
    input [15:0] TanhData;
    output [15:0] TanhResult;
    output [11:0] TanhAddress;
    output Saturated;
    input RomAddrSelect;

    reg [19:0] OriginalData;
    reg [19:0] D;
    reg [16:0] TanX;
    reg [16:0] TanX0;
    reg [16:0] TanX1;
    reg [15:0] TanhResult;
    wire [11:0] TanhAddress;
    reg [15:0] S;

    wire [15:0] Multiplicand;
    wire [15:0] Multiplier;
    reg  [31:0] Sum;
    reg  [15:0] Result;

    wire [15:0] x1minusx;
    wire [15:0] xminusx0;
    wire [15:0] minusselect;

    integer i;

    reg TanhAvailable;
    wire Saturated;

    reg capped;

    /* clock the data into the TANH module when its available */
    always @(posedge clk)
    begin
        if (WorkAvailable)
            OriginalData <= Data;
    end

    /* convert negatives to positive. */
    always @(*)
    begin
        if (OriginalData[19])
            D = (~OriginalData) + 1;
        else
            D = OriginalData;
    end
    assign Saturated = (|D[18:17]); //flag for saturation (should use saturated output)

	/* save the X,X1,X0 for interpolation later */
    always @(posedge clk)
    begin
        TanX <= D[16:0];
        TanX0 <= D[16:9] << 9;
        TanX1 <= (D[16:9] + 1) << 9;
    end
	/* create the wires that are input into hardware functions */
    assign x1minusx = TanX1 - TanX;
    assign xminusx0 = TanX - TanX0;
    assign TanhAddress = (RomAddrSelect) ? (D[16:9]+1) << 1 : D[16:9] << 1;
    assign minusselect = (RomAddrSelect) ? x1minusx: xminusx0;
    assign Multiplier = minusselect;
    assign Multiplicand = TanhData;

	/* multiply TANH ROM output and the minus */
    always @(Multiplicand, Multiplier)
    begin
        Sum = 0;
        for (i = 0; i <= 14; i = i + 1) begin
            if (Multiplier[i]) Sum = Sum + (Multiplicand << i);
        end
        if (Multiplier[15]) Sum = Sum + ((~Multiplicand + 1) << 15);
		Sum = Sum << 6;
        Result = Sum[30:15];
    end

    /* sum the interpolation values */
    always @(posedge clk)
    begin
        if (RomAddrSelect)
            S <= Result;
        else
            S <= S + Result;
    end

    /* the output, if its saturated the output is the max or min value,
        depending on if the input is + or - */
    always @(*)
    begin
        if (Saturated) begin
            if (OriginalData[19]) TanhResult = 16'b1000000000000001;
                else TanhResult = 16'b0111111111111111;
        end
        else begin
            if (OriginalData[19]) TanhResult = (~S + 1);
                else TanhResult = S;
        end
    end

endmodule

module Matrix(clk, reset, Xdata, Gdata, VecSelect, Out);
    input clk;
    input reset;
    input [15:0] Xdata;
    input [15:0] Gdata;
    input [3:0]  VecSelect;

    output [19:0] Out;

    reg [19:0] S;

    reg [31:0] Multiplicand;
    reg [15:0] Multiplier;
    reg [31:0] Sum;
    reg [19:0] Result;

    reg [19:0] Out;

    integer i;

	/* set the multiplier and multiplicand */
    always @(*)
    begin
        Multiplier = Xdata;
        Multiplicand = {{16{Gdata[15]}}, Gdata};
    end

	/* multiply */
    always @(Multiplicand, Multiplier)
    begin
        Sum = 0;
        for (i = 0; i <= 14; i = i + 1) begin
            if (Multiplier[i]) Sum = Sum + (Multiplicand << i);
        end
        if (Multiplier[15]) Sum = Sum + ((~Multiplicand + 1) << 15);
        Result = {{4{Sum[30]}}, Sum[30:15]};
    end

	/* accumulate */
    always @(posedge clk)
    begin
        if (VecSelect == 0) begin
            S <= Result;
        end else begin
            S <= Result + S;
        end
    end

    always @(*)
        Out = S;

endmodule

module Controller(clk, reset, run, Busy, Xaddress, Gaddress, Waddress,
                VecSelect, MatrixAvailable,
                TanhAvailable, WriteEnable, Saturated, TanhRomAddrSel);
    /* Inputs */
    input clk;
    input reset;
    input run;
    input Saturated;

    /* Outputs */
    output [11:0] Xaddress;
    output [11:0] Gaddress;
    output [11:0] Waddress;
    //output [3:0]  Row;
    output [3:0]  VecSelect;
    output        Busy;
    output        MatrixAvailable;
    output   TanhAvailable;
    output WriteEnable;
    output TanhRomAddrSel;

    /* Signals */
    reg [15:0] Count;
    reg [11:0] Xaddress;
    wire [11:0] Gaddress;
    reg [11:0] Waddress;
    //reg [3:0]  Row;
    reg [3:0]  VecSelect;
    reg        Busy;
    wire       IntReset;
    reg MatrixAvailable;

    wire  TanhAvailable;
    reg WriteEnable;
    reg TanhRomAddrSel;

    /* states for the tanh module */
    parameter [2:0]
        WaitForData = 3'b000,
        DataIsAvailable = 3'b001,
        CheckSaturationLoadRom0 = 3'b010,
        FirstAddLoadX1Addr = 3'b011,
        SecondAdd = 3'b100,
        WriteToSram = 3'b111;

    reg [2:0] current_state, next_state;

    /* Internal signals */
    wire ResultAvailable;

    always @(posedge clk)
    begin
        if (!reset || IntReset) Busy <= 0;
        else if (run) Busy <= 1;
    end

    always @(posedge clk)
    begin
        if (!reset || IntReset) Count <= 0;
        else if (Busy) Count <= Count + 1;
    end

    always @(posedge clk)
    begin
        //Row <= Count[7:4];
        VecSelect <= Count[3:0]; /* matrix multiplication index */
    end
	
    /* address for the SRAM inputs */
    always @(*)
    begin
        Xaddress = {Count[11:8], Count[3:0]} << 1;
    end
 
    /* address for the weights */
    assign Gaddress = Count[7:0] << 1;
    /* matrix result is available when the counter is > 16, and count[3:0] = 0*/
    assign ResultAvailable = (|Count[12:4]) & (Count[3:0] == 0);
    //assign TanhControl = (|Count[12:4]) ? Count[3:0] : 0;

    /* state machine function */
    always @(posedge clk)
    begin
        if (!reset || IntReset) current_state <= WaitForData;
        else current_state <= next_state;
    end

    always @(*)
    begin
        MatrixAvailable = 0;
        WriteEnable = 0;
        TanhRomAddrSel = 0;
        case (current_state)
        WaitForData: begin /* wait for matrix calc to complete */
            if (ResultAvailable) next_state = DataIsAvailable;
            else next_state = WaitForData;
            end
        DataIsAvailable: begin /* onces available, capture at tanh module */
                MatrixAvailable = 1;
                next_state = CheckSaturationLoadRom0;
            end
        CheckSaturationLoadRom0: begin
            /* If its saturated the output is available immediately,
               if not, this state sets the first tanh data to be available on the next clock edge */
                if (Saturated) next_state = WriteToSram;
                    else next_state = FirstAddLoadX1Addr;
        end
        FirstAddLoadX1Addr: begin //first addition happens here then sets up for next tanh data
            TanhRomAddrSel = 1;
            next_state = SecondAdd;
        end
        SecondAdd: begin //second add happens here, and the output is available
            next_state = WriteToSram;
        end
        WriteToSram: begin //write output to the sram (this state also increments the write address)
                WriteEnable = 1;
                next_state = WaitForData;
            end
        default: next_state = WaitForData;
        endcase
    end

    /* set the write-to address */
    always @(posedge clk)
    begin
        if (!reset || IntReset) Waddress <= 12'h200;
        else if (current_state == WriteToSram)
            Waddress <= Waddress + 2;
    end

	/* the round is over when the write address is set to 0x400 */
    assign IntReset = (Waddress == 12'h400);
endmodule
