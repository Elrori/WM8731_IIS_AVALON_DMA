/************************************************************************
*   Name:           16bit IIS DMA Driver for wm8731
*   Interface:      Avalon mm slave with master DMA.
*   Diagram:                          ___________
*                                    |u IIS DMA o|<- coe_export_IIS_BCLK
*                                    |s         u|<- coe_export_IIS_DACLRC
*                  avalon mm slave =>|e |fifo|  t|-> coe_export_IIS_DACDAT
*       avalon mm master (pipline)<=>|r |512 |   |<- coe_export_IIS_ADCLRC(not use)
*                                    |  |32b |   |<- coe_export_IIS_ADCDAT(not use)
*
*   Important:      master 从SDRAM(etc.)取数据满足 avalon mm master pipline read 时序
*                   参考 mnl_avalon_spec.pdf 第29页。以下逻辑满足从SDRAM规定地址
*                   读规定数量字节到DAC FIFO 的时序。逻辑复位后进入空闲状态，空闲状
*                   下，逻辑不断加载0x1地址数据到计数器初值。Go/Stop_n置1后，进入读区
*                   传送数据状态，读取完规定数据后再次进入空闲状态并使Go/Stop_n置0，
*                   IRQ flag置1；用户检测到IRQ flag置1或Go/Stop_n变为0或发生中断，
*                   必须清除IRQ flag位。然后再置Go/Stop_n位开始下次传送。
*                   
*   Origin:         171002
*                   180124
*                   180205
*   Author:         helrori2011@gmail.com
*
*                         AVALON SLAVE REGISTER MAP                                 
*   
*   Offset Name               Function
*   =================================================================================================================
*                            |                              |       8            ..         0   || R/W || default  ||
*   =================================================================================================================
*    0x0 STATUS              |                              |              DAC FIFO used        ||  R  || FFFFFE00 ||
*   -------------------------+------------------------------------------------------------------++-----++----------++
*    0x1 READ ADDR START     |                          RAM ADDRESS                             || R/W ||    0     ||
*   -------------------------+------------------------------------------------------------------++-----++----------++
*    0x2 READ DATA LEN(BYTES)|                    READ DATA LENGTH(BYTES)                       || R/W ||    0     ||
*   -------------------------+------------------------------------------------------------------++-----++----------++
*    0x3 FIFO THRESHOLD      |                          |FIFO too little threshold number(6-512)|| R/W || FFFFFE00 ||
*   =================================================================================================================
*                            |         |        3       |      2    |     1     |        0      ||     ||          ||
*   =================================================================================================================
*    0x4 CONTRAL REG         |         |  FIFO   reset  |  IRQ flag |   IRQ en  |  Go/Stop_n    || R/W || FFFFFFF0 ||
*   -------------------------+------------------------------------------------------------------++-----++----------++
*   DAC FIFO used:  DAC FIFO 占用字数。
*   RAM ADDRESS:    传送起始地址。
*   READ DATA LENGTH(BYTES):传送字节长度。
*   FIFO too little threshold number(6-512):FIFO阈值,范围(6-512),应大于FIFO深度的一半。
*   FIFO reset:     置1异步复位DACFIFO，置0恢复DACFIFO。CONTRAL REG低4位置0用来给通用寄存器复位。
*   IRQ flag:       中断标志,一次传送结束自动置1。
*   IRQ en:         使能IRQ state的中断。
*   Go/Stop_n:      置1开始传送,一次传送结束自动置0。
*
*   Operation process:      1.Set RAM ADDRESS
*                           2.Clear IRQ flag
*                           3.Set Go/Stop_n
*                           4.Check IRQ flag
**************************************************************************/
module IIS_driver_DMA_top
#(
    parameter DAC_FIFO_DEPTH = 512
)
(
    //avs mm interface
    input               reset_n,
    input               clk,
    input               avs_s1_chipselect,
    input   [2:0]       avs_s1_address,
    input   [3:0]       avs_s1_byteenable,
    input               avs_s1_write,
    input   [31:0]      avs_s1_writedata,
    input               avs_s1_read,    
    output  reg[31:0]   avs_s1_readdata,
    output              avs_s1_irq,
    //avm mm interface for DMA,connect to SDRAM
    output  [31:0]      avm_s1_address,
    input   [31:0]      avm_s1_readdata,
    output              avm_s1_read,
    input               avm_s1_waitrequest,
    input               avm_s1_data_valid,
    output  [3:0]       avm_s1_byteenable,
    //output pin    
    input               coe_export_IIS_BCLK,
    input               coe_export_IIS_DACLRC,
    input               coe_export_IIS_ADCLRC,              //not use
    input               coe_export_IIS_ADCDAT,              //not use
    output              coe_export_IIS_DACDAT
    
);
//function integer log2;
//   input [31:0] value;
//   for (log2=0; value>0; log2=log2+1)
//     value = value>>1;
//endfunction
localparam              DAC_FIFO_WIDTH = 32;
wire                    rst_n;
assign                  rst_n = reset_n;
wire                    [9-1:0]wrusedw;
reg                     coe_export_IIS_DACLRC_Delay_1_coe_export_IIS_BCLK;
reg                     [5:0]Sel_Cnt;
wire                    [DAC_FIFO_WIDTH-1:0]q;
reg                     IIS_DACDAT_OUT_EN;
reg                     [7:0]cnt;


wire        [31:0]SREG;     //STATUS
reg     [31:0]ADDRREG;  //START ADDRESS
reg     [31:0]LENREG;   //READ DATA LEN(BYTES)
reg     [31:0]LESSREG;  //FIFO THRESHOLD 
reg     [31:0]CREG;     //CONTRAL REG

reg     [31:0]address_counter;
reg     trans_over;

//reg       less_half_wr_over;
reg     fifo_little_ref;
reg     fifo_little_ref_dealy;
wire    fifo_little;

wire    areset;
wire    go_bit;
wire    irq;
wire    irq_en;
wire    counter_en;


assign  go_bit = CREG[0];
assign  irq_en = CREG[1];
assign  irq      = CREG[2];
assign  areset = CREG[3];
assign  SREG   = wrusedw;

initial
begin
    cnt       = 8'd0;
    IIS_DACDAT_OUT_EN = 1;
end
/********************************************************************
*   avalon mm slave
*********************************************************************/
assign avs_s1_irq = irq & irq_en;
//  WR
always@(posedge clk or negedge rst_n)
    if(!rst_n)
        CREG[0] <= 0;//go_bit = 0
    else//  if trans_over clear go_bit
        CREG[0] <= ( avs_s1_write && avs_s1_chipselect && (avs_s1_address == 4))?avs_s1_writedata[0]:( trans_over?1'b0:CREG[0]);
always@(posedge clk or negedge rst_n)
    if(!rst_n)
        CREG[1] <= 0;//irq_en = 0
    else
        CREG[1] <= ( avs_s1_write && avs_s1_chipselect && (avs_s1_address == 4))?avs_s1_writedata[1]:CREG[1];       
always@(posedge clk or negedge rst_n)
    if(!rst_n)
        CREG[2] <= 0;//irq = 0
    else//  if trans_over set irq
        CREG[2] <= ( avs_s1_write && avs_s1_chipselect && (avs_s1_address == 4))?avs_s1_writedata[2]:( trans_over?1'b1:CREG[2]);
always@(posedge clk or negedge rst_n)
    if(!rst_n)
        CREG[3] <= 0;//areset = 0
    else
        CREG[3] <= ( avs_s1_write && avs_s1_chipselect && (avs_s1_address == 4))?avs_s1_writedata[3]:CREG[3];               
always@(posedge clk or negedge rst_n)begin
    if(!rst_n)begin
        ADDRREG <= 32'd0;
        LENREG  <= 32'd0;
        LESSREG <= 32'd0;
        end
    else if(avs_s1_write && avs_s1_chipselect)
        case(avs_s1_address)
//      3'd0:
        3'd1:   begin
                if(avs_s1_byteenable[0]) ADDRREG[7:0]       <= avs_s1_writedata[7:0];
                if(avs_s1_byteenable[1]) ADDRREG[15:8]      <= avs_s1_writedata[15:8];
                if(avs_s1_byteenable[2]) ADDRREG[23:16]     <= avs_s1_writedata[23:16];
                if(avs_s1_byteenable[3]) ADDRREG[31:24]     <= avs_s1_writedata[31:24];
                end
        3'd2:   begin
                if(avs_s1_byteenable[0]) LENREG[7:0]    <= avs_s1_writedata[7:0];
                if(avs_s1_byteenable[1]) LENREG[15:8]   <= avs_s1_writedata[15:8];
                if(avs_s1_byteenable[2]) LENREG[23:16]  <= avs_s1_writedata[23:16];
                if(avs_s1_byteenable[3]) LENREG[31:24]  <= avs_s1_writedata[31:24];     
                end
        3'd3: begin
                if(avs_s1_byteenable[0]) LESSREG[7:0]       <= avs_s1_writedata[7:0];
                if(avs_s1_byteenable[1]) LESSREG[15:8]      <= avs_s1_writedata[15:8];
                if(avs_s1_byteenable[2]) LESSREG[23:16]     <= avs_s1_writedata[23:16];
                if(avs_s1_byteenable[3]) LESSREG[31:24]     <= avs_s1_writedata[31:24];     
                end
//      3'd4:
        default:;
        endcase
end
//  RD
always@(*)begin
    if(avs_s1_chipselect && avs_s1_read)
        case(avs_s1_address)
        3'd0:avs_s1_readdata = {23'h7fffff,SREG[8:0]};
        3'd1:avs_s1_readdata = ADDRREG;
        3'd2:avs_s1_readdata = LENREG;
        3'd3:avs_s1_readdata = {23'h7fffff,LESSREG[8:0]};
        3'd4:avs_s1_readdata = {28'hfffffff,CREG[3:0]};
        default:avs_s1_readdata = 32'hffffffff;
        endcase
end

/********************************************************************
*   avalon mm pipline read master DMA,参考mnl_avalon_spec.pdf 第29页。
*********************************************************************/
//地址计数器
always@(posedge clk or negedge rst_n)begin
    if(!rst_n)begin
        address_counter     <= 32'd0;
        trans_over          <= 1'd0;
    end
    else if((go_bit == 1'd0) || trans_over == 1'd1)begin//load addr and idle state
        address_counter     <= ADDRREG;
        trans_over          <= 1'd0;

    end
    else if((avm_s1_read == 1'd1) && (avm_s1_waitrequest == 1'd0) && (go_bit == 1))//write address state
        if(address_counter < (ADDRREG + LENREG - 4))//trans
            address_counter <= address_counter + 32'd4;
        else begin  //  trans over set trans_over and the next clk will go to idle state ( always in idle state until cpu set go_bit) 
                        //  and clear trans_over,clear go_bit,set irq.
            trans_over      <= 1'd1;
        end
    else
        address_counter <= address_counter;
end
/*
*   go_bit==1 且 fifo数据小于阈值需尽快置avm_s1_read。 
*   时钟驱动fifo_little信号，减少毛刺亚稳态
*/
assign  fifo_little = (wrusedw < (LESSREG[8:0] - 9'd5));
always@(posedge clk,negedge rst_n)begin
    if(!rst_n)
        fifo_little_ref <= 1'd0;
    else
        fifo_little_ref <= fifo_little;
end
always@(posedge clk or negedge rst_n)begin
    if(!rst_n)
        fifo_little_ref_dealy <= 1'd0;
    else
        fifo_little_ref_dealy <= fifo_little_ref;
end
assign avm_s1_read       = /*fifo_little_ref_dealy*/state && go_bit && (trans_over == 1'd0);//state or fifo_little_ref_dealy任选其一作为读信号使能
assign avm_s1_address    = address_counter;
assign avm_s1_byteenable = 'hff;//DMA从SDRAM基地址每次自增4(也就是每隔一个字)读取一个字，byteenable全选
/*
*   input logic,小于一半连续写满 状态机
*   state:  当LESSREG < 512-15时:
*               当wrusedw小于LESSREG，连续读SDRAM到FIFO直到写满FIFO
                使用state时LESSREG可以控制一次从SDRAM的数据数量，大约是512-LESSREG
*           当LESSREG > 512-15时：
*               永不休止的读SDRAM(一次一个时钟，写满停一下),FIFO always full
*   fifo_little_ref_dealy:当wrusedw小于LESSREG时读取几个数据到FIFO
*   
*   使用fifo_little_ref_dealy或state改变LESSREG值可以改善avalon mm 总线读写冲突
*/
reg state;
always@(posedge clk or negedge rst_n)begin
    if(!rst_n)
        state   <=  0;
    else if(state   ==  0   &&  wrusedw <   LESSREG)begin//<510
        state   <=  1;
    end
    else if(state   ==  1   &&  wrusedw >   512-15)begin//almost full>495
        state   <=  0;
    end
    else
        state   <=  state;
end
/********************************************************************
*   fifo 512*32bit{L,R};输入字数据按照16bitWAV格式排列
*********************************************************************/
fifo fifo_U2(//quartus ip
    .aclr(areset),
    .data({avm_s1_readdata[15:8],avm_s1_readdata[7:0],avm_s1_readdata[31:24],avm_s1_readdata[23:16]}),
    .rdclk(coe_export_IIS_DACLRC),
    .rdreq(1),              //输出连续
    .wrclk(clk),
    .wrreq(avm_s1_data_valid),
    .q(q),
    .rdempty(),
    .wrusedw(wrusedw));

/********************************************************************
*   output logic,以下为对应 DSP模式首bit空模式
*********************************************************************/
assign coe_export_IIS_DACDAT= (IIS_DACDAT_OUT_EN)?q[DAC_FIFO_WIDTH-1-Sel_Cnt]:1'd0;//高位先
always@(posedge coe_export_IIS_BCLK)
    coe_export_IIS_DACLRC_Delay_1_coe_export_IIS_BCLK <= coe_export_IIS_DACLRC;
always@(negedge coe_export_IIS_BCLK or negedge rst_n)
begin
    if(!rst_n)
        Sel_Cnt <= 6'd0;
    else if(coe_export_IIS_DACLRC_Delay_1_coe_export_IIS_BCLK == 1)//在DSP模式首空bit处（DSP模式设置成首bit空模式），及时清零帧内计数器
        Sel_Cnt <= 6'd0;
    else    
        Sel_Cnt <= Sel_Cnt + 6'd1;
end
always@(negedge coe_export_IIS_BCLK or negedge rst_n)
begin
    if(~rst_n)begin
        IIS_DACDAT_OUT_EN <= 1;
        cnt                 <= 7'd0;
    end
    else if(coe_export_IIS_DACLRC_Delay_1_coe_export_IIS_BCLK == 1)
    begin
        cnt <= 7'd0;
        IIS_DACDAT_OUT_EN <= 1;
    end
    else if(cnt == DAC_FIFO_WIDTH-1)
    begin
        cnt <= 7'd0;
        IIS_DACDAT_OUT_EN <= 0;
    end
    else
    begin
        cnt <= cnt + 7'd1;
        IIS_DACDAT_OUT_EN <= IIS_DACDAT_OUT_EN;
    end
end
endmodule
