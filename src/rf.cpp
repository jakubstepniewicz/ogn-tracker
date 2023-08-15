#include <stdint.h>

#include "rf.h"
#include "gps.h"
#include "timesync.h"

// uint32_t RX_Random = 0x12345678;

FreqPlan  RF_FreqPlan;               // frequency hopping pattern calculator

       // FIFO<Manch_RxPktData, 16> RF_RxFIFO;         // buffer for received packets
       FIFO<OGN_TxPacket<OGN_Packet>, 4> RF_TxFIFO;   // buffer for transmitted packets

#ifdef WITH_ADSL
       FIFO<ADSL_Packet, 4> ADSL_TxFIFO;
#endif

#ifdef WITH_FANET
       FIFO<FANET_RxPacket, 8> FNT_RxFIFO;
       FIFO<FANET_Packet, 4> FNT_TxFIFO;
#endif

       uint8_t RX_OGN_Packets=0;            // [packets] counts received packets

       int32_t TX_Credit  = 0;              // [ms] counts transmitter time avoid using more than 1% or air time
       uint16_t RX_OGN_Count64=0;           // counts received packets for the last 64 seconds

       RFM_TRX           TRX;

// OGNv1 SYNC:       0x0AF3656C encoded in Manchester
static const uint8_t OGN1_SYNC[10] = { 0xAA, 0x66, 0x55, 0xA5, 0x96, 0x99, 0x96, 0x5A, 0x00, 0x00 };
static const uint8_t *OGN_SYNC = OGN1_SYNC;
// ADS-L SYNC:       0xF5724B18 encoded in Manchester (fixed packet length 0x18 is included)
static const uint8_t ADSL_SYNC[10] = { 0x55, 0x99, 0x95, 0xA6, 0x9A, 0x65, 0xA9, 0x6A, 0x00, 0x00 };
// PilotAware SYNC, includes net-address which is always zero, and the packet size which is always 0x18 = 24
static const uint8_t PAW_SYNC [10] = { 0xB4, 0x2B, 0x00, 0x00, 0x00, 0x00, 0x18, 0x71, 0x00, 0x00 };

 void vTaskRF(void* pvParameters)
{
  // RF_RxFIFO.Clear();                      // clear receive/transmit packet FIFO's
  RF_TxFIFO.Clear();
#ifdef WITH_FANET
  FNT_RxFIFO.Clear();
  FNT_TxFIFO.Clear();
#endif
  TRX.Init();

  RF_FreqPlan.setPlan(Parameters.FreqPlan);     // 1 = Europe/Africa, 2 = USA/CA, 3 = Australia and South America
  TRX.BaseFrequency = RF_FreqPlan.BaseFreq;
  TRX.ChannelSpacing = RF_FreqPlan.ChanSepar;
  TRX.FreqCorr = Parameters.RFchipFreqCorr;

  vTaskDelay(1000);

  for( ; ; )
  {
    for( ; TimeSync_msTime()<400; )
    { vTaskDelay(1); }
    TRX.ConfigManchFSK(26, OGN_SYNC, 8);                         // configure for OGN
    TRX.setOutputPower(Parameters.TxPower);
    TRX.setChannel(1, 1);

    const uint8_t *TxPktData0=0;
    const uint8_t *TxPktData1=0;
    const OGN_TxPacket<OGN_Packet> *TxPkt0 = RF_TxFIFO.getRead(0);             // get 1st packet from TxFIFO
    const OGN_TxPacket<OGN_Packet> *TxPkt1 = RF_TxFIFO.getRead(1);             // get 2nd packet from TxFIFO
    if(TxPkt0) TxPktData0=TxPkt0->Byte();                                      // if 1st is not NULL then get its data
    if(TxPkt1) TxPktData1=TxPkt1->Byte();                                      // if 2nd if not NULL then get its data
          else TxPktData1=TxPktData0;                                          // but if NULL then take copy of the 1st packet

    TRX.ManchSlot(400, TxPktData0);

    TRX.setChannel(0, 1);
    TRX.ManchSlot(400, TxPktData1);

    if(TxPkt0) RF_TxFIFO.Read();
    if(TxPkt1) RF_TxFIFO.Read();

  }

}
