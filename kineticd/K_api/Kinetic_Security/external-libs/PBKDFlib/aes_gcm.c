#if CRYPTO_MATH_AES_GCM
//-----------------------------------------------------------------------------
//
// Header: Math/sc_aes.c
// Date: 2013/07/12
// Author: tim.j.courtney
//
// Description: This is trust module aes_gcm code. .
//
//-----------------------------------------------------------------------------

//
// Do NOT modify or remove this copyright and confidentiality notice!
//
// Copyright (c) 2001-2014 Seagate Technology, LLC.
//
// The code contained herein is CONFIDENTIAL to Seagate Technology, LLC.
// Portions are also trade secret. Any use, duplication, derivation, distribution
// or disclosure of this code, for any reason, not expressly authorized is
// prohibited. All other rights are expressly reserved by Seagate Technology, LLC.
//
//--------------------------------------------------------------------------
//
// Portable C version of AES_GCM functions
//
//--------------------------------------------------------------------------

//******************************************************************************
/*****ADDED CA ***/
#include <string.h>
#include "sc_pbkdf.h"
#include "aes_gcm.h"
#include "sc_aes.h"
#include "sc_aescrypt.h"


//******************************************************************************
//#define RUN_AES_GCM_GALOIS_MULTIPLY_TESTS  1
//#define RUN_AES_GCM_TEST_VECTORS        1
//#define AES_GCM_VERBOSE_OUTPUT          0

//******************************************************************************
typedef struct
{
   uint32 PCLen;
   uint16 PCPad;
   uint32 ALen;
   uint16 APad;
   uint32 NumPCBlocks;
   uint32 NumABlocks;
}  a_gcm_v_set;

//******************************************************************************
//aes_rval aes_enc_key(const unsigned char in_key[], unsigned int klen, aes_ctx cx[1]);
//aes_rval aes_enc_blk(const unsigned char in_blk[], unsigned char out_blk[], const aes_ctx cx[1]);

//******************************************************************************
void my_memcpy( uint8 *to, uint8 *from, uint16 len )
{
   uint16 ii;
   for ( ii=0; ii<len; ii++ )
   {
      *(to+ii) = *(from+ii);
   }
}

//******************************************************************************
int EncryptECB_Mode( aes_ctx *ctx, uint8 *PlainText, uint8 *CypherText, int DataLen )
{
   uint8    buf[BLOCK_LEN];
   uint8    ebuf[BLOCK_LEN];
   int      NumBlocks, Remainder;
   int      ii;

   NumBlocks = DataLen / BLOCK_LEN;
   Remainder = DataLen % BLOCK_LEN;
   for ( ii = 0; ii < NumBlocks; ii++ )                  // read the file a block at a time
   {
      my_memcpy( buf, PlainText+ii*BLOCK_LEN, BLOCK_LEN );
      aes_enc_blk( buf, ebuf, ctx );                     // encrypt the block
      my_memcpy( CypherText+ii*BLOCK_LEN, ebuf, BLOCK_LEN );
   }
   if ( Remainder )
   {
      my_memcpy( buf, PlainText+NumBlocks*BLOCK_LEN, BLOCK_LEN );
      aes_enc_blk( buf, ebuf, ctx );                     // encrypt the block
      my_memcpy( CypherText+NumBlocks*BLOCK_LEN, ebuf, BLOCK_LEN );
   }
   return 0;
}

//******************************************************************************
void M_DebugPrintHex8( uint8 Data )
{
   uint8 temp;
   char tStr[3];

   tStr[2] = 0;

   temp = Data & 0x0F;
   if (temp<0x0A) tStr[1] = temp + '0';
   else tStr[1] = temp + 0x37;

   temp = Data >> 4;
   if (temp<0x0A) tStr[0] = temp + '0';
   else tStr[0] = temp + 0x37;
   M_DebugXmitString( tStr );
}

//******************************************************************************
void M_DebugPrintHex32( uint32 Data )
{
   uint8 temp;

   temp = (uint8) ((Data & 0xFF000000)>>24);
   M_DebugPrintHex8( temp );
   temp = (uint8) ((Data & 0x00FF0000)>>16);
   M_DebugPrintHex8( temp );
   temp = (uint8) ((Data & 0x0000FF00)>>8);
   M_DebugPrintHex8( temp );
   temp = (uint8) (Data & 0x000000FF);
   M_DebugPrintHex8( temp );
}

//******************************************************************************
void M_DebugPrintData( char *DataName, uint8 *Data, int DataLen )
{
   int      ii;

   M_DebugXmitString( DataName );
   M_DebugXmitString( "                   " );
   for ( ii=0; ii < DataLen; ii++ )
   {
      if ( ( ii%BLOCK_LEN==0 ) && ( ii!=0 ) )
         M_DebugXmitString( "\r\n                    " );
      M_DebugPrintHex8( Data[ii] );
   }
   M_DebugXmitString( "\r\n" );
}

//******************************************************************************
void uint8_ptr_to_uint32_ptr( uint8 *p8, uint32 *p32 )
{
   int ii;

   for (ii=0; ii<4; ii++ )
   {
      p32[ii] = ( *(p8+ii*4+3)<<24 ) + ( *(p8+ii*4+2)<<16 ) + ( *(p8+ii*4+1)<<8 ) + *(p8+ii*4+0);
   }
}

//******************************************************************************
void uint32_ptr_to_uint8_ptr( uint32 *p32, uint8 *p8 )
{
   int ii;
   uint32 temp;

   for (ii=0; ii<4; ii++ )
   {
      temp = *(p32+ii);
      p8[ii*4+0] = temp & 0x000000FF;
      p8[ii*4+1] = (temp & 0x0000FF00) >> 8;
      p8[ii*4+2] = (temp & 0x00FF0000) >> 16;
      p8[ii*4+3] = (temp & 0xFF000000) >> 24;
   }
}

//******************************************************************************
void uint8_swap_bits( uint8 *p8 )
{
   int ii;
   uint8 temp;

   for (ii=0; ii<16; ii++ )
   {
      temp = 0;
      if (*(p8+ii) & 0x01) temp |= 0x80;
      if (*(p8+ii) & 0x02) temp |= 0x40;
      if (*(p8+ii) & 0x04) temp |= 0x20;
      if (*(p8+ii) & 0x08) temp |= 0x10;
      if (*(p8+ii) & 0x10) temp |= 0x08;
      if (*(p8+ii) & 0x20) temp |= 0x04;
      if (*(p8+ii) & 0x40) temp |= 0x02;
      if (*(p8+ii) & 0x80) temp |= 0x01;
      *(p8+ii) = temp;
   }
}

//******************************************************************************
uint8 get_a_byte( uint8 *asc )
{
   uint8 v1, val;

   v1 = *(asc+0) - '0';
   if ( v1 > 9 ) v1 -= 0x07;
   if ( v1 > 0x0f ) v1 -= 0x20;
   val = *(asc+1) - '0';
   if ( val > 9 ) val -= 0x07;
   if ( val > 0x0f ) val -= 0x20;
   val = val | (v1<<4);

   return val & 0xFF;
}

//******************************************************************************
uint8 swap_a_byte( uint8 *asc )
{
   uint8    temp;
   uint16      v1, val;

   v1 = *(asc+0) - '0';
   if ( v1 > 9 ) v1 -= 0x07;
   if ( v1 > 0x0f ) v1 -= 0x20;
   val = *(asc+1) - '0';
   if ( val > 9 ) val -= 0x07;
   if ( val > 0x0f ) val -= 0x20;
   val = val | (v1<<4);

   temp = 0;
   if ( val&0x01 ) temp |= 0x80;
   if ( val&0x02 ) temp |= 0x40;
   if ( val&0x04 ) temp |= 0x20;
   if ( val&0x08 ) temp |= 0x10;
   if ( val&0x10 ) temp |= 0x08;
   if ( val&0x20 ) temp |= 0x04;
   if ( val&0x40 ) temp |= 0x02;
   if ( val&0x80 ) temp |= 0x01;
   return temp;
}

//******************************************************************************
void asc_to_uint32_ptr( uint8 *asc, uint32 *p32, uint16 Len )
{
   int      ii;
   uint32      temp;

   for ( ii=0; ii<Len; ii+=8 )
   {
      temp = swap_a_byte(asc+ii+6) << 24;
      temp |= swap_a_byte(asc+ii+4) << 16;
      temp |= swap_a_byte(asc+ii+2) << 8;
      temp |= swap_a_byte(asc+ii+0);
      *p32++ = temp;
   }
}

//******************************************************************************
void asc_to_uint8_ptr( uint8 *asc, uint8 *p8, uint16 Len )
{
   int      ii;
   uint32      temp;

   for ( ii=0; ii<Len; ii+=2 )
   {
      temp = get_a_byte(asc+ii);
      *p8++ = temp;
   }
}

#if RUN_AES_GCM_TEST_VECTORS
//******************************************************************************
//                         "0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF"
uint8 K_0[]  =             "88c9b3db5c7c7a135eb1b939efae1be8b3f0303bdd515f101f5d9836635d47c4";
uint8 P_0[]  =             "d8f6fa";
uint8 A_0[]  =             "759119";
uint8 IV_0[] =             "5eea64e99a40ba72fb7879e7";
uint8 C_0[]  =             "104cd3";
uint8 T_0[]  =             "c083d2a34d01f8af06b587ff4d9448e8";

uint8 K_1[] =              "00000000000000000000000000000000";
uint8 P_1[] =              "";
uint8 A_1[] =              "";
uint8 IV_1[] =             "000000000000000000000000";
uint8 H_1[] =              "66e94bd4ef8a2c3b884cfa59ca342b2e";
uint8 Y0_1[] =             "00000000000000000000000000000001";
uint8 E_K_Y0_1[] =         "58e2fccefa7e3061367f1d57a4e7455a";
uint8 lenA_cat_lenC_1[] =  "00000000000000000000000000000000";
uint8 GHASH_HAC_1[] =      "00000000000000000000000000000000";
uint8 C_1[] =              "";
uint8 T_1[] =              "58e2fccefa7e3061367f1d57a4e7455a";

//                         "0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF"
uint8 K_2[] =              "00000000000000000000000000000000";
uint8 P_2[] =              "00000000000000000000000000000000";
uint8 A_2[] =              "";
uint8 IV_2[] =             "000000000000000000000000";
uint8 H_2[] =              "66e94bd4ef8a2c3b884cfa59ca342b2e";
uint8 Y0_2[] =             "00000000000000000000000000000001";
uint8 EK_Y0_2[] =          "58e2fccefa7e3061367f1d57a4e7455a";
uint8 Y1_2[] =             "00000000000000000000000000000002";
uint8 EK_Y1_2[] =          "0388dace60b6a392f328c2b971b2fe78";
uint8 X1_2[] =             "5e2ec746917062882c85b0685353deb7";
uint8 lenA_cat_lenC_2[] =  "00000000000000000000000000000080";
uint8 GHASH_HAC_2[] =      "f38cbb1ad69223dcc3457ae5b6b0f885";
uint8 C_2[] =              "0388dace60b6a392f328c2b971b2fe78";
uint8 T_2[] =              "ab6e47d42cec13bdf53a67b21257bddf";

//                         "0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF"
uint8 K_3[] =              "feffe9928665731c6d6a8f9467308308";
uint8 P_3[] =              "d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a721c3c0c95956809532fcf0e2449a6b525b16aedf5aa0de657ba637b391aafd255";
uint8 A_3[] =              "";
uint8 IV_3[] =             "cafebabefacedbaddecaf888";
uint8 H_3[] =              "b83b533708bf535d0aa6e52980d53b78";
uint8 Y0_3[] =             "cafebabefacedbaddecaf88800000001";
uint8 E_K_Y0_3[] =         "3247184b3c4f69a44dbcd22887bbb418";
uint8 Y1_3[] =             "cafebabefacedbaddecaf88800000002";
uint8 E_K_Y1_3[] =         "9bb22ce7d9f372c1ee2b28722b25f206";
uint8 Y2_3[] =             "cafebabefacedbaddecaf88800000003";
uint8 E_K_Y2_3[] =         "650d887c3936533a1b8d4e1ea39d2b5c";
uint8 Y3_3[] =             "cafebabefacedbaddecaf88800000004";
uint8 E_K_Y3_3[] =         "3de91827c10e9a4f5240647ee5221f20";
uint8 Y4_3[] =             "cafebabefacedbaddecaf88800000005";
uint8 E_K_Y4_3[] =         "aac9e6ccc0074ac0873b9ba85d908bd0";
uint8 X1_3[] =             "59ed3f2bb1a0aaa07c9f56c6a504647b";
uint8 X2_3[] =             "b714c9048389afd9f9bc5c1d4378e052";
uint8 X3_3[] =             "47400c6577b1ee8d8f40b2721e86ff10";
uint8 X4_3[] =             "4796cf49464704b5dd91f159bb1b7f95";
uint8 lenA_cat_lenC_3[] =  "00000000000000000000000000000200";
uint8 GHASH_HAC_3[] =      "7f1b32b81b820d02614f8895ac1d4eac";
uint8 C_3[] =              "42831ec2217774244b7221b784d0d49ce3aa212f2c02a4e035c17e2329aca12e21d514b25466931c7d8f6a5aac84aa051ba30b396a0aac973d58e091473f5985";
uint8 T_3[] =              "4d5c2af327cd64a62cf35abd2ba6fab4";

//                         "0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF"
uint8 K_4[] =              "feffe9928665731c6d6a8f9467308308";
uint8 P_4[] =              "d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a721c3c0c95956809532fcf0e2449a6b525b16aedf5aa0de657ba637b39";
uint8 A_4[] =              "feedfacedeadbeeffeedfacedeadbeefabaddad2";
uint8 IV_4[] =             "cafebabefacedbaddecaf888";
uint8 H_4[] =              "b83b533708bf535d0aa6e52980d53b78";
uint8 Y0_4[] =             "cafebabefacedbaddecaf88800000001";
uint8 E_K_Y0_4[] =         "3247184b3c4f69a44dbcd22887bbb418";
uint8 X1_4[] =             "ed56aaf8a72d67049fdb9228edba1322";
uint8 X2_4[] =             "cd47221ccef0554ee4bb044c88150352";
uint8 Y1_4[] =             "cafebabefacedbaddecaf88800000002";
uint8 E_K_Y1_4[] =         "9bb22ce7d9f372c1ee2b28722b25f206";
uint8 Y2_4[] =             "cafebabefacedbaddecaf88800000003";
uint8 E_K_Y2_4[] =         "650d887c3936533a1b8d4e1ea39d2b5c";
uint8 Y3_4[] =             "cafebabefacedbaddecaf88800000004";
uint8 E_K_Y3_4[] =         "3de91827c10e9a4f5240647ee5221f20";
uint8 Y4_4[] =             "cafebabefacedbaddecaf88800000005";
uint8 E_K_Y4_4[] =         "aac9e6ccc0074ac0873b9ba85d908bd0";
uint8 X3_4[] =             "54f5e1b2b5a8f9525c23924751a3ca51";
uint8 X4_4[] =             "324f585c6ffc1359ab371565d6c45f93";
uint8 X5_4[] =             "ca7dd446af4aa70cc3c0cd5abba6aa1c";
uint8 X6_4[] =             "1590df9b2eb6768289e57d56274c8570";
uint8 lenA_cat_lenC_4[] =  "00000000000000a00000000000000";
uint8 GHASH_HAC_4[] =      "1e0 698e57f70e6ecc7fd9463b7260a9ae5f";
uint8 C_4[] =              "42831ec2217774244b7221b784d0d49ce3aa212f2c02a4e035c17e2329aca12e21d514b25466931c7d8f6a5aac84aa051ba30b396a0aac973d58e091";
uint8 T_4[] =              "5bc94fbc3221a5db94fae95ae7121a4729";

//                         "0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF"
uint8 K_13[] =             "0000000000000000000000000000000000000000000000000000000000000000";
uint8 P_13[] =             "";
uint8 A_13[] =             "";
uint8 IV_13[] =            "000000000000000000000000";
uint8 H_13[] =             "dc95c078a2408989ad48a21492842087";
uint8 Y0_13[] =            "00000000000000000000000000000001";
uint8 EK_Y0_13[] =         "530f8afbc74536b9a963b4f1c4cb738b";
uint8 lenA_cat_lenC_13[] = "00000000000000000000000000000000";
uint8 GHASH_HAC_13[] =     "00000000000000000000000000000000";
uint8 C_13[] =             "";
uint8 T_13[] =             "530f8afbc74536b9a963b4f1c4cb738b";

//                         "0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF"
uint8 K_14[] =             "0000000000000000000000000000000000000000000000000000000000000000";
uint8 P_14[] =             "00000000000000000000000000000000";
uint8 A_14[] =             "";
uint8 IV_14[] =            "000000000000000000000000";
uint8 H_14[] =             "dc95c078a2408989ad48a21492842087";
uint8 Y0_14[] =            "00000000000000000000000000000001";
uint8 EK_Y0_14[] =         "530f8afbc74536b9a963b4f1c4cb738b";
uint8 Y1_14[] =            "00000000000000000000000000000002";
uint8 EK_Y1_14[] =         "cea7403d4d606b6e074ec5d3baf39d18";
uint8 X1_14[] =            "fd6ab7586e556dba06d69cfe6223b262";
uint8 lenA_cat_lenC_14[] = "00000000000000000000000000000080";
uint8 GHASH_HAC_14[] =     "83de425c5edc5d498f382c441041ca92";
uint8 C_14[] =             "cea7403d4d606b6e074ec5d3baf39d18";
uint8 T_14[] =             "d0d1c8a799996bf0265b98b5d48ab919";

//                         "0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF"
uint8 K_15[] =             "feffe9928665731c6d6a8f9467308308feffe9928665731c6d6a8f9467308308";
uint8 P_15[] =             "d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a721c3c0c95956809532fcf0e2449a6b525b16aedf5aa0de657ba637b391aafd255";
uint8 A_15[] =             "";
uint8 IV_15[] =            "cafebabefacedbaddecaf888";
uint8 H_15[] =             "acbef20579b4b8ebce889bac8732dad7";
uint8 Y0_15[] =            "cafebabefacedbaddecaf88800000001";
uint8 EK_Y0_15[] =         "fd2caa16a5832e76aa132c1453eeda7e";
uint8 Y1_15[] =            "cafebabefacedbaddecaf88800000002";
uint8 EK_Y1_15[] =         "8b1cf3d561d27be251263e66857164e7";
uint8 Y2_15[] =            "cafebabefacedbaddecaf88800000003";
uint8 EK_Y2_15[] =         "e29d258faad137135bd49280af645bd8";
uint8 Y3_15[] =            "cafebabefacedbaddecaf88800000004";
uint8 EK_Y3_15[] =         "908c82ddcc65b26e887f85341f243d1d";
uint8 Y4_15[] =            "cafebabefacedbaddecaf88800000005";
uint8 EK_Y4_15[] =         "749cf39639b79c5d06aa8d5b932fc7f8";
uint8 X1_15[] =            "fcbefb78635d598eddaf982310670f35";
uint8 X2_15[] =            "29de812309d3116a6eff7ec844484f3e";
uint8 X3_15[] =            "45fad9deeda9ea561b8f199c3613845b";
uint8 X4_15[] =            "ed95f8e164bf3213febc740f0bd9c6af";
uint8 lenA_cat_LenC_15[] = "00000000000000000000000000000200";
uint8 GHASH_HAC_15[] =     "4db870d37cb75fcb46097c36230d1612";
uint8 C_15[] =             "522dc1f099567d07f47f37a32a84427d643a8cdcbfe5c0c97598a2bd2555d1aa8cb08e48590dbb3da7b08b1056828838c5f61e6393ba7a0abcc9f662898015ad";
uint8 T_15[] =             "b094dac5d93471bdec1a502270e3cc6c";

//                         "0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF"
uint8 K_16[] =             "feffe9928665731c6d6a8f9467308308feffe9928665731c6d6a8f9467308308";
uint8 P_16[] =             "d9313225f88406e5a55909c5aff5269a86a7a9531534f7da2e4c303d8a318a721c3c0c95956809532fcf0e2449a6b525b16aedf5aa0de657ba637b39";
uint8 A_16[] =             "feedfacedeadbeeffeedfacedeadbeefabaddad2";
uint8 IV_16[] =            "cafebabefacedbaddecaf888";
uint8 H_16[] =             "acbef20579b4b8ebce889bac8732dad7";
uint8 Y0_16[] =            "cafebabefacedbaddecaf88800000001";
uint8 EK_Y0_16[] =         "fd2caa16a5832e76aa132c1453eeda7e";
uint8 X1_16[] =            "5165d242c2592c0a6375e2622cf925d2";
uint8 X2_16[] =            "8efa30ce83298b85fe71abefc0cdd01d";
uint8 Y1_16[] =            "cafebabefacedbaddecaf88800000002";
uint8 EK_Y1_16[] =         "8b1cf3d561d27be251263e66857164e7";
uint8 Y2_16[] =            "cafebabefacedbaddecaf88800000003";
uint8 EK_Y2_16[] =         "e29d258faad137135bd49280af645bd8";
uint8 Y3_16[] =            "cafebabefacedbaddecaf88800000004";
uint8 EK_Y3_16[] =         "908c82ddcc65b26e887f85341f243d1d";
uint8 Y4_16[] =            "cafebabefacedbaddecaf88800000005";
uint8 EK_Y4_16[] =         "749cf39639b79c5d06aa8d5b932fc7f8";
uint8 X3_16[] =            "abe07e0bb62354177480b550f9f6cdcc";
uint8 X4_16[] =            "3978e4f141b95f3b4699756b1c3c2082";
uint8 X5_16[] =            "8abf3c48901debe76837d8a05c7d6e87";
uint8 X6_16[] =            "9249beaf520c48b912fa120bbf391dc8";
uint8 lenA_cat_lenC_16[] = "00000000000000a000000000000001e0";
uint8 GHASH_HAC_16[] =     "8bd0c4d8aacd391e67cca447e8c38f65";
uint8 C_16[] =             "522dc1f099567d07f47f37a32a84427d643a8cdcbfe5c0c97598a2bd2555d1aa8cb08e48590dbb3da7b08b1056828838c5f61e6393ba7a0abcc9f662";
uint8 T_16[] =             "76fc6ece0f4e1768cddf8853bb2d551b";
#endif

//******************************************************************************
void GaloisMultiply_128( uint32 *a32, uint32 *b32, uint32 *p32 )
{
   static uint32 R32[BLOCK_LEN/sizeof(uint32)] = {0x00000087, 0x00000000, 0x00000000, 0x00000000};
   uint32 high_bit_was_set;
   uint32 ii;

   for (ii=0; ii<4; ii++ )
   {
      p32[ii] = 0;
   }
   for ( ii=0; ii<128; ii++ )
   {
      if ( b32[0] & 0x00000001 )
      {
         p32[0] = p32[0] ^ a32[0];
         p32[1] = p32[1] ^ a32[1];
         p32[2] = p32[2] ^ a32[2];
         p32[3] = p32[3] ^ a32[3];
      }

      high_bit_was_set = a32[3] & 0x80000000;
      a32[3] = a32[3] << 1;         // The next 10 lines perform a 128 bit arithmetic shift left
      if ( a32[2] & 0x80000000 )
         a32[3] |= 0x00000001;
      a32[2] = a32[2] << 1;
      if ( a32[1] & 0x80000000 )
         a32[2] |= 0x00000001;
      a32[1] = a32[1] << 1;
      if ( a32[0] & 0x80000000 )
         a32[1] |= 0x00000001;
      a32[0] = a32[0] << 1;

      if ( high_bit_was_set )       // if high bit was set xor with poly
      {
         a32[0] = a32[0] ^ R32[0];
         a32[1] = a32[1] ^ R32[1];
         a32[2] = a32[2] ^ R32[2];
         a32[3] = a32[3] ^ R32[3];
      }
      b32[0] = b32[0] >> 1;         // The next 10 lines perform a 128 bit arithmetic shift right
      if ( b32[1] & 0x00000001 )
         b32[0] |= 0x80000000;
      b32[1] = b32[1] >> 1;
      if ( b32[2] & 0x00000001 )
         b32[1] |= 0x80000000;
      b32[2] = b32[2] >> 1;
      if ( b32[3] & 0x00000001 )
         b32[2] |= 0x80000000;
      b32[3] = b32[3] >> 1;
   }
}

//******************************************************************************
void GaloisMultiply_128_Test( uint32 *a32, uint32 *b32, uint32 *p32 )
{
   M_DebugXmitString( "0x" );
   M_DebugPrintHex32( a32[3] );
   M_DebugPrintHex32( a32[2] );
   M_DebugPrintHex32( a32[1] );
   M_DebugPrintHex32( a32[0] );
   M_DebugXmitString( " X 0x" );
   M_DebugPrintHex32( b32[3] );
   M_DebugPrintHex32( b32[2] );
   M_DebugPrintHex32( b32[1] );
   M_DebugPrintHex32( b32[0] );
   M_DebugXmitString( " = 0x" );

   GaloisMultiply_128( a32, b32, p32 );

   M_DebugPrintHex32( p32[3] );
   M_DebugPrintHex32( p32[2] );
   M_DebugPrintHex32( p32[1] );
   M_DebugPrintHex32( p32[0] );
   M_DebugXmitString( "\r\n" );
}

//******************************************************************************
#if RUN_AES_GCM_GALOIS_MULTIPLY_TESTS
void MultiplyCharPtr( uint8 *a, uint8 *b, uint8 *o )
{
   int ii;
   static uint8  a8[16];
   static uint8  b8[16];
   static uint32 a32[BLOCK_LEN/sizeof(uint32)];
   static uint32 b32[BLOCK_LEN/sizeof(uint32)];
   static uint32 p32[BLOCK_LEN/sizeof(uint32)];

   my_memcpy( a8, a, BLOCK_LEN );
   my_memcpy( b8, b, BLOCK_LEN );
   uint8_swap_bits( a8 );
   uint8_ptr_to_uint32_ptr( a8, a32 );
   uint8_swap_bits( b8 );
   uint8_ptr_to_uint32_ptr( b8, b32 );
   GaloisMultiply_128_Test( a32, b32, p32 );

   if (o==NULL) return;

   uint8_swap_bits( o );
   M_DebugXmitString( "                                                                      o = 0x" );
   for (ii=0; ii<BLOCK_LEN; ii++)
      M_DebugPrintHex8( o[15-ii] );
   M_DebugXmitString( "\r\n" );
}

//******************************************************************************
void MultiplyAscii( uint8 *as, uint8 *bs, uint8 *o )
{
   int ii;
   static uint32 a32[BLOCK_LEN/sizeof(uint32)];
   static uint32 b32[BLOCK_LEN/sizeof(uint32)];
   static uint32 p32[BLOCK_LEN/sizeof(uint32)];

   asc_to_uint32_ptr( as, a32, 32 );
   asc_to_uint32_ptr( bs, b32, 32 );
   GaloisMultiply_128_Test( a32, b32, p32 );

   if (o==NULL) return;

   M_DebugXmitString( "                                                                      o = 0x" );
   for (ii=0; ii<BLOCK_LEN; ii++)
      M_DebugPrintHex8( o[15-ii] );
   M_DebugXmitString( "\r\n" );
}

//******************************************************************************
//                                                                                                         b     b
//                                                                                                         1     1
//                    b     b                                                                              1     2
//                    0     8                                                                              9     7
uint8 a1[BLOCK_LEN] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
uint8 b1[BLOCK_LEN] = {0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

uint8 a2[BLOCK_LEN] = {0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
uint8 b2[BLOCK_LEN] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};

uint8 a3[BLOCK_LEN] = {0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
uint8 b3[BLOCK_LEN] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF};

uint8 a4[BLOCK_LEN] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF};
uint8 b4[BLOCK_LEN] = {0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

uint8 a5[BLOCK_LEN] = {0x66, 0xe9, 0x4b, 0xd4, 0xef, 0x8a, 0x2c, 0x3b, 0x88, 0x4c, 0xfa, 0x59, 0xca, 0x34, 0x2b, 0x2e};
uint8 b5[BLOCK_LEN] = {0x03, 0x88, 0xda, 0xce, 0x60, 0xb6, 0xa3, 0x92, 0xf3, 0x28, 0xc2, 0xb9, 0x71, 0xb2, 0xfe, 0x78};
uint8 o5[BLOCK_LEN] = {0x5e, 0x2e, 0xc7, 0x46, 0x91, 0x70, 0x62, 0x88, 0x2c, 0x85, 0xb0, 0x68, 0x53, 0x53, 0xde, 0xb7};

uint8 a6[BLOCK_LEN] = {0xb8, 0x3b, 0x53, 0x37, 0x08, 0xbf, 0x53, 0x5d, 0x0a, 0xa6, 0xe5, 0x29, 0x80, 0xd5, 0x3b, 0x78};
uint8 b6[BLOCK_LEN] = {0x42, 0x83, 0x1e, 0xc2, 0x21, 0x77, 0x74, 0x24, 0x4b, 0x72, 0x21, 0xb7, 0x84, 0xd0, 0xd4, 0x9c};
uint8 o6[BLOCK_LEN] = {0x59, 0xed, 0x3f, 0x2b, 0xb1, 0xa0, 0xaa, 0xa0, 0x7c, 0x9f, 0x56, 0xc6, 0xa5, 0x04, 0x64, 0x7b};

//******************************************************************************
//                     1         2         3
//            12345678901234567890123456789012
uint8 as1[] = "00000000000000000000000000000001";
uint8 bs1[] = "40000000000000000000000000000000";

uint8 as2[] = "40000000000000000000000000000000";
uint8 bs2[] = "00000000000000000000000000000001";

uint8 as3[] = "40000000000000000000000000000000";
uint8 bs3[] = "000000000000000000000000000000FF";

uint8 as4[] = "000000000000000000000000000000FF";
uint8 bs4[] = "40000000000000000000000000000000";

uint8 as5[] = "66e94bd4ef8a2c3b884cfa59ca342b2e";
uint8 bs5[] = "0388dace60b6a392f328c2b971b2fe78";
uint8 os5[] = "5e2ec746917062882c85b0685353deb7";

uint8 as6[] = "b83b533708bf535d0aa6e52980d53b78";
uint8 bs6[] = "42831ec2217774244b7221b784d0d49c";
uint8 os6[] = "59ed3f2bb1a0aaa07c9f56c6a504647b";

//******************************************************************************
void MultiplyTest( void )
{
   M_DebugXmitString( "***********************************************************************************************************\r\n" );
   M_DebugXmitString( "Galois Multiply Tests:\r\n" );

   MultiplyCharPtr( a1, b1, NULL );
   MultiplyAscii( as1, bs1, NULL );
   M_DebugXmitString( "\r\n" );

   MultiplyCharPtr( a2, b2, NULL );
   MultiplyAscii( as2, bs2, NULL );
   M_DebugXmitString( "\r\n" );

   MultiplyCharPtr( a3, b3, NULL );
   MultiplyAscii( as3, bs3, NULL );
   M_DebugXmitString( "\r\n" );

   MultiplyCharPtr( a4, b4, NULL );
   MultiplyAscii( as4, bs4, NULL );
   M_DebugXmitString( "\r\n" );

   MultiplyCharPtr( a5, b5, o5 );
   MultiplyAscii( as5, bs5, o5 );
   M_DebugXmitString( "\r\n" );

   MultiplyCharPtr( a6, b6, o6 );
   MultiplyAscii( as6, bs6, o6 );
   M_DebugXmitString( "\r\n" );

   M_DebugXmitString( "***********************************************************************************************************\r\n" );
}
#endif

//******************************************************************************
void IncIV( uint8 *p )
{
   *(p+15) += 1;
   if ( *(p+15) == 0 )
   {
      *(p+14) += 1;
      if ( *(p+14) == 0 )
      {
         *(p+13) += 1;
         if ( *(p+13) == 0 )
            *(p+12) += 1;
      }
   }
}

//******************************************************************************
void Xor128_Bit( uint32 *S1, uint32 *S2, uint32* Ans )
{
   int ii;

   for (ii=0; ii<4; ii++ )
   {
      Ans[ii] = S1[ii] ^ S2[ii];
   };
}

//******************************************************************************
void MakeLenACatLenC( a_gcm_v_set *pgcm_vars, uint8 *pp)
{
   int ii;
   uint32 uint32Temp;

   for ( ii=0; ii<BLOCK_LEN; ii++ )
   {
      *(pp+ii) = 0;
   }
   uint32Temp = pgcm_vars->ALen*8;
   *(pp+7) = (uint8)uint32Temp;
   uint32Temp >>= 8;
   *(pp+6) = (uint8)uint32Temp;
   uint32Temp >>= 8;
   *(pp+5) = (uint8)uint32Temp;
   uint32Temp >>= 8;
   *(pp+4) = (uint8)uint32Temp;

   uint32Temp = pgcm_vars->PCLen*8;
   *(pp+15) = (uint8)uint32Temp;
   uint32Temp >>= 8;
   *(pp+14) = (uint8)uint32Temp;
   uint32Temp >>= 8;
   *(pp+13) = (uint8)uint32Temp;
   uint32Temp >>= 8;
   *(pp+12) = (uint8)uint32Temp;
}

//******************************************************************************
//       _
//      | 0                                for i = 0
//      | (Xi-1 ^ Ai) . H                  for i = 1,...,m-1
// Xi = | (Xm-1 ^ (Am || 0^128-v)) . H     for i = m
//      | (Xi-1 ^ Ci) . H                  for i = m+1,...,m+n-1
//      | (Xm+n-1 ^ (Cm || 0^128-u)) . H   for i = m+n
//      |_(Xm+n ^ (len(A)||len(C))) . H    for i = m+n+1
//
void GHASH( uint8 *H8, uint8 *A8, uint8 *C8, uint8 *X8, uint32 Xlen, a_gcm_v_set *pgcm_vars )
{
   uint32 ii, jj;
   uint32 t32[BLOCK_LEN/sizeof(uint32)];
   uint32 H32[BLOCK_LEN/sizeof(uint32)];
   uint32 ANS32[BLOCK_LEN/sizeof(uint32)];
   static uint8 t8[BLOCK_LEN];
   static uint8 temp[BLOCK_LEN];

#if AES_GCM_VERBOSE_OUTPUT
   M_DebugXmitString( "  X = GHASHh( A || 0ev || C || 0eu || [len( A )64] || [len( C )64] )\r\n" );
#endif
   uint8_swap_bits( H8 );

   //*****************************************************************************
   // This section implements
   // Xi = 0                                for i = 0
   for ( ii=0; ii<BLOCK_LEN; ii++ ) X8[ii] = 0;

   //*****************************************************************************
   // This section implements
   // Xi = (Xi-1 ^ Ai) . H                  for i = 1,...,m-1
   // Xi = (Xm-1 ^ (Am || 0^128-v)) . H     for i = m
   for ( ii=0; ii<pgcm_vars->NumABlocks; ii++ )
   {
#if AES_GCM_VERBOSE_OUTPUT
      M_DebugPrintData( "X8", X8+ii*BLOCK_LEN, BLOCK_LEN );
      M_DebugPrintData( "X8", X8, BLOCK_LEN );
#endif
      for (jj=0; jj<BLOCK_LEN; jj++ )
         t8[jj] = X8[jj] ^ A8[ii*BLOCK_LEN+jj];
      uint8_swap_bits( t8 );
      uint8_ptr_to_uint32_ptr( t8, t32 );
      uint8_ptr_to_uint32_ptr( H8, H32 );
      GaloisMultiply_128( H32, t32, ANS32 );
      uint32_ptr_to_uint8_ptr( ANS32, t8 );
      uint8_swap_bits( t8 );
#if AES_GCM_VERBOSE_OUTPUT
      M_DebugPrintData( "t8", t8, BLOCK_LEN );
#endif
      my_memcpy( X8, t8, BLOCK_LEN );
   }

   //*****************************************************************************
   // This section implements
   // Xi = (Xi-1 ^ Ci) . H                  for i = m+1,...,m+n-1
   // Xi = (Xm+n-1 ^ (Cm || 0^128-u)) . H   for i = m+n
   for ( ii=0; ii<pgcm_vars->NumPCBlocks; ii++ )
   {
#if AES_GCM_VERBOSE_OUTPUT
      M_DebugPrintData( "X8", X8+(pgcm_vars->NumABlocks+ii)*BLOCK_LEN, BLOCK_LEN );
      M_DebugPrintData( "X8", X8, BLOCK_LEN );
#endif
      for (jj=0; jj<BLOCK_LEN; jj++ )
         t8[jj] = X8[jj] ^ C8[ii*BLOCK_LEN+jj];
      uint8_swap_bits( t8 );
      uint8_ptr_to_uint32_ptr( t8, t32 );
      uint8_ptr_to_uint32_ptr( H8, H32 );
      GaloisMultiply_128( H32, t32, ANS32 );
      uint32_ptr_to_uint8_ptr( ANS32, t8 );
      uint8_swap_bits( t8 );
#if AES_GCM_VERBOSE_OUTPUT
      M_DebugPrintData( "t8", t8, BLOCK_LEN );
#endif
      my_memcpy( X8, t8, BLOCK_LEN );
   }

   //*****************************************************************************
   // Xi = (Xm+n ^ (len(A)||len(C))) . H    for i = m+n+1
   MakeLenACatLenC( pgcm_vars, temp );
#if AES_GCM_VERBOSE_OUTPUT
   M_DebugPrintData( "temp = ", temp, BLOCK_LEN );
   M_DebugPrintData( "X8", X8, BLOCK_LEN );
#endif
   for (jj=0; jj<BLOCK_LEN; jj++ )
   {
      t8[jj] = X8[jj] ^ temp[jj];
   }
   uint8_swap_bits( t8 );
   uint8_ptr_to_uint32_ptr( t8, t32 );
   uint8_ptr_to_uint32_ptr( H8, H32 );
   GaloisMultiply_128( H32, t32, ANS32 );
   uint32_ptr_to_uint8_ptr( ANS32, t8 );
   uint8_swap_bits( t8 );
#if AES_GCM_VERBOSE_OUTPUT
   M_DebugPrintData( "t8", t8, BLOCK_LEN );
#endif
   my_memcpy( X8, t8, BLOCK_LEN );
#if AES_GCM_VERBOSE_OUTPUT
   M_DebugPrintData( "X8", X8, BLOCK_LEN );
#endif
   uint8_swap_bits( H8 );
}

//******************************************************************************
int PadData( uint8 *PC8, uint32 xPCLen, uint8 *A8, uint32 ALen, a_gcm_v_set *pgcm )
{
   uint32 ii;

   static uint8 Y8[BLOCK_LEN];
   static uint8 EkY8[BLOCK_LEN];
   static uint8 EkY8_0Block[BLOCK_LEN];
   static uint8 H8[BLOCK_LEN];
   static uint8 X8[BLOCK_LEN];
   static uint8 TempBlock[BLOCK_LEN];

#if AES_GCM_VERBOSE_OUTPUT
   M_DebugXmitString( "***********************************************************************************************************\r\n" );
   M_DebugXmitString("IN: PadData()\r\n" );
#endif
   pgcm->PCLen = xPCLen;
   pgcm->ALen = ALen;

#if AES_GCM_VERBOSE_OUTPUT
   M_DebugXmitString( "\r\n***********************************************************************************************************\r\n" );
   M_DebugXmitString( "STEP 1) Pad Plain text data\r\n" );
#endif
   pgcm->PCPad = 0;
   if  ( pgcm->PCLen != 0 )
   {
      pgcm->PCPad = pgcm->PCLen % BLOCK_LEN;
      if ( pgcm->PCPad > 0 ) pgcm->PCPad = BLOCK_LEN - pgcm->PCPad;
   }
   for ( ii=0; ii<(pgcm->PCPad); ii++ )
      PC8[pgcm->PCLen+ii] = 0;
   pgcm->NumPCBlocks = (pgcm->PCLen+pgcm->PCPad) / BLOCK_LEN;
#if AES_GCM_VERBOSE_OUTPUT
   M_DebugPrintData( "PC8", PC8, pgcm->PCLen + pgcm->PCPad );
#endif

#if AES_GCM_VERBOSE_OUTPUT
   M_DebugXmitString( "\r\n***********************************************************************************************************\r\n" );
   M_DebugXmitString( "STEP 2) Pad additional authenticated data\r\n" );
#endif
   pgcm->APad = 0;
   if  ( pgcm->ALen != 0 )
   {
      pgcm->APad = pgcm->ALen % BLOCK_LEN;
      if ( pgcm->APad > 0 ) pgcm->APad = BLOCK_LEN - pgcm->APad;
   }
   for ( ii=0; ii<(pgcm->APad); ii++ )
      A8[pgcm->ALen+ii] = 0;
   pgcm->NumABlocks = (pgcm->ALen+pgcm->APad) / BLOCK_LEN;
#if AES_GCM_VERBOSE_OUTPUT
   M_DebugPrintData( "A8", A8, pgcm->ALen + pgcm->APad );
#endif

   return NO_ERROR;
}

//******************************************************************************
void EncryptZeroBlock( uint8 *H8, uint8 *K8, uint16 KLen )
{
   uint16 ii;
   aes_ctx ctx[1];
   static uint8 TempBlock[BLOCK_LEN];

#if AES_GCM_VERBOSE_OUTPUT
   M_DebugXmitString( "\r\n***********************************************************************************************************\r\n" );
   M_DebugXmitString( "STEP 3) Let H = E(K, 0^128 )\r\n" );
#endif
   for ( ii=0; ii<BLOCK_LEN; ii++ ) TempBlock[ii] = 0;
#if AES_GCM_VERBOSE_OUTPUT
   M_DebugPrintData( "K", K8, KLen );
   M_DebugPrintData( "0^128", TempBlock, BLOCK_LEN );
#endif
   aes_enc_key(K8, KLen, ctx );
   EncryptECB_Mode( ctx, TempBlock, H8, BLOCK_LEN );
#if AES_GCM_VERBOSE_OUTPUT
   M_DebugPrintData( "H", H8, BLOCK_LEN );
#endif
}

//******************************************************************************
void DefineY0Block (uint8 *Y8, uint8 *IV8, uint16 IVLen )
{
#if AES_GCM_VERBOSE_OUTPUT
   M_DebugXmitString( "\r\n***********************************************************************************************************\r\n" );
   M_DebugXmitString( "STEP 4) Define a block Y0 as follows:\r\n" );
   M_DebugXmitString( "  Y0 = IV || 0^31 || 1\r\n" );
#endif
   my_memcpy( Y8, IV8, IVLen );
   Y8[12] = 0x00;
   Y8[13] = 0x00;
   Y8[14] = 0x00;
   Y8[15] = 0x01;
#if AES_GCM_VERBOSE_OUTPUT
   M_DebugPrintData( "Y0", Y8, BLOCK_LEN );
#endif
}

//******************************************************************************
// It is important to realize that there is no decryption in AES CTR mode.  Just call
// EncryptGCTR_ModePY() with the plain text P8 and the Cypher test C8 swapped.
// Encrypt -> EncryptGCTR_ModePY( P8, C8, K8, Y8, EkY8, EkY8_0Block, KLen, &gcm_vars );
// Decrypt -> EncryptGCTR_ModePY( C8, P8, K8, Y8, EkY8, EkY8_0Block, KLen, &gcm_vars );
void EncryptGCTR_ModePY( uint8 *P8, uint8 *C8, uint8 *K8, uint8 *Y8, uint8 *EkY8, uint8 *EkY8_0Block, uint16 KLen, a_gcm_v_set *pgcm )
{
   int zz;
   uint16 ii, jj;
   aes_ctx ctx[1];

#if AES_GCM_VERBOSE_OUTPUT
   M_DebugXmitString( "\r\n***********************************************************************************************************\r\n" );
   M_DebugXmitString( "STEP 5) Let C = P ^ E( K, Yi )\r\n" );
   M_DebugPrintData( "K8", K8, BLOCK_LEN );
   M_DebugPrintData( "Y0", Y8, BLOCK_LEN );
   M_DebugXmitString( "\r\n" );
#endif

   aes_enc_key( K8, KLen, ctx );
   for ( ii=0; ii<(pgcm->NumPCBlocks+1); ii++ )
   {
      EncryptECB_Mode( ctx, Y8, EkY8, BLOCK_LEN );
      if ( ii==0 ) my_memcpy( EkY8_0Block, EkY8, BLOCK_LEN );

      if ( ii > 0 )
      {
         for( jj=0; jj<BLOCK_LEN; jj++ )                 // do CBC chaining prior to encryption
            C8[ (ii-1)*BLOCK_LEN+jj ] = EkY8[jj] ^ P8[ (ii-1) * BLOCK_LEN + jj ];
      }
      IncIV( Y8 );
   }
}

//******************************************************************************
void GenerateT( uint8 *T8, uint8 *X8, uint8 *EkY8_0Block, a_gcm_v_set *pgcm )
{
   uint16 ii;

#if AES_GCM_VERBOSE_OUTPUT
   M_DebugXmitString( "\r\n***********************************************************************************************************\r\n" );
   M_DebugXmitString( "STEP 8: Let\r\n" );
   M_DebugXmitString( "  T = MSBt( GCTRk( Y0, X ) )\r\n" );
   M_DebugPrintData( "T8[ii] = ", X8+(pgcm->NumABlocks+pgcm->NumPCBlocks+1)*BLOCK_LEN, BLOCK_LEN );
#endif
   for ( ii=0; ii<BLOCK_LEN; ii++ )
   {
      //T8[ii] = xX8[(pgcm->NumABlocks+pgcm->NumPBlocks+1)*BLOCK_LEN+ii] ^ EkY8_0Block[ii];
      T8[ii] = X8[ii] ^ EkY8_0Block[ii];
   }
}

//******************************************************************************
void WipePlaintextOrKey ( uint8 *S8, uint32 SLen )
{
   uint32 ii;

#if AES_GCM_VERBOSE_OUTPUT
   M_DebugXmitString( "\r\n***********************************************************************************************************\r\n" );
   M_DebugXmitString( "                    Wiping Plaintext or Key\r\n" );
   M_DebugPrintData( "S8", S8, SLen );
#endif
   for ( ii=0; ii<SLen; ii++ )
      S8[ii] = 0;
#if AES_GCM_VERBOSE_OUTPUT
   M_DebugPrintData( "S8", S8, SLen );
#endif
}

//******************************************************************************
// Inputs:  uint8  *P8     - Pointer to Plaintext (P) buffer
//          uint32 PLen    - P data length
//          uint8  *A8     - Address of Additional Authenticated Data (AAD) buffer
//          uint32 ALen    - AAD Length
//          uint8  *IV8    - Pointer to Initialization Vector (IV)
//          uint16 IVLen   - IV length
//          uint8  *K8     - Pointer to encryption Key (K)
//          uint16 KLen    - K length
//
// Outputs: uint8  *C8     - Pointer to Cyphertext (C) buffer
//          uint32 *CLen   - Pointer to variable to hold the length of the Cyphertext
//          uint8  *T8     - Pointer to Authentication Tag (T) buffer
//          uint16 *TLen   - Pointer to variable to hold the length of the Authentication Tag
//
// Returns: GCM_error         - NO_ERROR if no error
GCM_error EncryptGCM_Mode( uint8 *P8, uint32 PLen, uint8 *A8, uint32 ALen, uint8 *IV8, uint16 IVLen, uint8 *K8, uint16 KLen, uint8 *C8, uint32 *CLen, uint8 *T8, uint16 *TLen )
{
   uint32 ii;
   a_gcm_v_set gcm_vars;

   static uint8 Y8[BLOCK_LEN];
   static uint8 EkY8[BLOCK_LEN];
   static uint8 EkY8_0Block[BLOCK_LEN];
   static uint8 H8[BLOCK_LEN];
   static uint8 X8[BLOCK_LEN];
   static uint8 TempBlock[BLOCK_LEN];

   if ( PLen > FW_MAX_PCLEN ) return ERROR_PLAINTEXT_TOO_LARGE;
   if ( ALen > FW_MAX_ALEN ) return ERROR_ASSOCIATED_DATA_TOO_LARGE;

   //--------------------------------------------------------------------------
   // STEP 1) Pad plain text
   PadData( P8, PLen, A8, ALen, &gcm_vars );

   //--------------------------------------------------------------------------
   // STEP 2) Let H = E(K, 0^128 )
   EncryptZeroBlock( H8, K8, KLen );

   //--------------------------------------------------------------------------
   // STEP 3) Define a block Y0 as follows:
   //     Y0 = IV || 0^31 || 1\r\n" );
   DefineY0Block ( Y8, IV8, IVLen );

   //--------------------------------------------------------------------------
   // STEP 4) Let C = P ^ E( K, Yi )
   EncryptGCTR_ModePY( P8, C8, K8, Y8, EkY8, EkY8_0Block, KLen, &gcm_vars );

   //--------------------------------------------------------------------------
   // STEP 5) GHASH:
#if AES_GCM_VERBOSE_OUTPUT
   M_DebugPrintData( "A8", A8, gcm_vars.ALen+gcm_vars.APad );
#endif
   for ( ii=gcm_vars.PCLen; ii<(gcm_vars.PCLen+gcm_vars.PCPad); ii++ )
      C8[ii] = 0;
   GHASH( H8, A8, C8, X8, BLOCK_LEN, &gcm_vars );

   //--------------------------------------------------------------------------
   // STEP 6) Let T = MSBt( GCTRk( Y0, X ) )\r\n" );
   GenerateT( T8, X8, EkY8_0Block, &gcm_vars );

   //--------------------------------------------------------------------------
   // STEP 7) Set Cypher text and Tag return lenghts
   *TLen = BLOCK_LEN;
   *CLen = gcm_vars.PCLen;

   //--------------------------------------------------------------------------
   // STEP 8) Wipe encryption key
   //if ( WipeKey ) WipePlaintextOrKey( K8, KLen );

   return NO_ERROR;
}

//******************************************************************************
// Inputs:  uint8  *C8     - Pointer to Cyphertext (C) buffer
//          uint32 CLen    - C data length
//          uint8  *A8     - Address of Additional Authenticated Data (AAD) buffer
//          uint32 ALen    - AAD Length
//          uint8  *T8     - Pointer to Authentication Tag (T) buffer
//          uint16 TLen    - T length
//          uint8  *IV8    - Pointer to Initialization Vector (IV)
//          uint16 IVLen   - IV length
//          uint8  *K8     - Pointer to encryption Key (K)
//          uint16 KLen    - K length
//
// Outputs: uint8  *P8     - Pointer to Plaintext (P) buffer
//          uint32 *PLen   - Pointer to variable to hold the length of the Plaintext
//
// Returns: GCM_error        - NO_ERROR if no error,
GCM_error DecryptGCM_Mode( uint8 *C8, uint32 CLen, uint8 *A8, uint32 ALen, uint8 *T8, uint16 TLen, uint8 *IV8, uint16 IVLen, uint8 *K8, uint16 KLen, uint8 *P8, uint32 *PLen )
{
   uint32 ii;
   a_gcm_v_set gcm_vars;
   int GCM_DecryptionStatus = NO_ERROR;

   static uint8 Y8[BLOCK_LEN];
   static uint8 EkY8[BLOCK_LEN];
   static uint8 EkY8_0Block[BLOCK_LEN];
   static uint8 H8[BLOCK_LEN];
   static uint8 X8[BLOCK_LEN];
   static uint8 TempBlock[BLOCK_LEN];
   static uint8 T8Calc[BLOCK_LEN];

#if AES_GCM_VERBOSE_OUTPUT
   M_DebugXmitString( "***********************************************************************************************************\r\n" );
   M_DebugXmitString("IN: DecryptGCM_Mode()\r\n" );
#endif

   if ( CLen > FW_MAX_PCLEN ) return ERROR_CYPHERTEXT_TOO_LARGE;
   if ( ALen > FW_MAX_ALEN ) return ERROR_ASSOCIATED_DATA_TOO_LARGE;

   //--------------------------------------------------------------------------
   // STEP 1) Pad cypher text
   PadData( C8, CLen, A8, ALen, &gcm_vars );

   //--------------------------------------------------------------------------
   // STEP 2) Let H = E(K, 0^128 )
   EncryptZeroBlock( H8, K8, KLen );

   //--------------------------------------------------------------------------
   // STEP 3) Define a block Y0 as follows:
   //     Y0 = IV || 0^31 || 1\r\n" );
   DefineY0Block ( Y8, IV8, IVLen );

   //--------------------------------------------------------------------------
   // STEP 4) GHASH:
   for ( ii=gcm_vars.PCLen; ii<(gcm_vars.PCLen+gcm_vars.PCPad); ii++ )
      C8[ii] = 0;
   GHASH( H8, A8, C8, X8, BLOCK_LEN, &gcm_vars );

   //--------------------------------------------------------------------------
   // STEP 5) Let P = C ^ E( K, Yi )
   EncryptGCTR_ModePY( C8, P8, K8, Y8, EkY8, EkY8_0Block, KLen, &gcm_vars );

   //--------------------------------------------------------------------------
   // STEP 6) Let T = MSBt( GCTRk( Y0, X ) )\r\n" );
   GenerateT( T8Calc, X8, EkY8_0Block, &gcm_vars );

   //--------------------------------------------------------------------------
   // STEP 7) Set Cypher text and Tag return lenghts
   TLen = BLOCK_LEN;
   *PLen = gcm_vars.PCLen;

   //--------------------------------------------------------------------------
   // STEP 8) Wipe encryption key
   //if ( WipeKey ) WipePlaintextOrKey( K8, KLen );

   //--------------------------------------------------------------------------
   // STEP 9) Compare Authentication Tag (T) parameter with calculated Authentication Tag
   for ( ii=0; ii<BLOCK_LEN; ii++ )
   {
      if ( T8Calc[ii] != T8[ii] )
      {
#if AES_GCM_VERBOSE_OUTPUT
         M_DebugXmitString( "ERROR: Authentication tags do NOT match!\r\n" );
#endif
         GCM_DecryptionStatus = ERROR_AUTHENTICATION_TAGS_DO_NOT_MATCH;
         WipePlaintextOrKey( P8, gcm_vars.PCLen );
         break;
      }
   }

   //PrintResults (P8, C8, A8, K8, KLen, IV8, IVLen, T8, TLen, H8, X8, Y8, &gcm_vars );
   return GCM_DecryptionStatus;
}

//******************************************************************************
// Inputs:  uint8  *P     - Pointer to Plaintext (P) buffer
//          uint32 PLen    - P data length
//          uint8  *A     - Address of Additional Authenticated Data (AAD) buffer
//          uint32 ALen    - AAD Length
//          uint8  *IV    - Pointer to Initialization Vector (IV)
//          uint16 IVLen   - IV length
//          uint8  *K     - Pointer to encryption Key (K)
//          uint16 KLen    - K length
//
// Outputs: uint8  *C     - Pointer to Cyphertext (C) buffer
//          uint32 *CL   - Pointer to variable to hold the length of the Cyphertext
//          uint8  *T     - Pointer to Authentication Tag (T) buffer
//          uint16 *TL   - Pointer to variable to hold the length of the Authentication Tag
//
// Returns: GCM_error         - NO_ERROR if no error
GCM_error EncryptGCM_Mode_Wrapper( uint8 *P, uint32 PLen, uint8 *A, uint32 ALen, uint8 *IV, uint16 IVLen, uint8 *K, uint16 KLen, uint8 *C, uint32 *CL, uint8 *T, uint16 *TL )
{
   GCM_error ErrorStatus;
   static uint8 P8[FW_MAX_PCLEN];
   static uint8 A8[FW_MAX_ALEN];
   static uint8 K8[MAX_KEY_LEN];
   static uint8 IV8[BLOCK_LEN];

   static uint8 C8[FW_MAX_PCLEN];
   static uint8 T8[BLOCK_LEN];
   uint32 CLen;
   uint16 TLen;

   M_Memcpy( P8, P, PLen );
   M_Memcpy( A8, A, ALen );
   M_Memcpy( K8, K, KLen );
   M_Memcpy( IV8, IV, IVLen );
   CLen = *CL;
   TLen = *TL;

   ErrorStatus = EncryptGCM_Mode( P8, PLen, A8, ALen, IV8, IVLen, K8, KLen, C8, &CLen, T8, &TLen );

   *CL = CLen;
   M_Memcpy( C, C8, *CL );
   M_Memcpy( T, T8, 16 );

   memset( P8, 0, FW_MAX_PCLEN );
   memset( A8, 0, FW_MAX_ALEN );
   memset( K8, 0, MAX_KEY_LEN );
   memset( IV8, 0, BLOCK_LEN );
   memset( C8, 0, FW_MAX_PCLEN );
   memset( T8, 0, BLOCK_LEN );

   return ( ErrorStatus );
}

//******************************************************************************
// Inputs:  uint8  *C     - Pointer to Cyphertext (C) buffer
//          uint32 CLen    - C data length
//          uint8  *A     - Address of Additional Authenticated Data (AAD) buffer
//          uint32 ALen    - AAD Length
//          uint8  *T     - Pointer to Authentication Tag (T) buffer
//          uint16 TLen    - T length
//          uint8  *IV    - Pointer to Initialization Vector (IV)
//          uint16 IVLen   - IV length
//          uint8  *K     - Pointer to encryption Key (K)
//          uint16 KLen    - K length
//
// Outputs: uint8  *P     - Pointer to Plaintext (P) buffer
//          uint32 *PL   - Pointer to variable to hold the length of the Plaintext
//
// Returns: GCM_error        - NO_ERROR if no error,
GCM_error DecryptGCM_Mode_Wrapper( uint8 *C, uint32 CLen, uint8 *A, uint32 ALen, uint8 *T, uint16 TLen, uint8 *IV, uint16 IVLen, uint8 *K, uint16 KLen, uint8 *P, uint32 *PL )
{
   GCM_error ErrorStatus;

   static uint8 P8[FW_MAX_PCLEN];
   static uint8 A8[FW_MAX_ALEN];
   static uint8 K8[MAX_KEY_LEN];
   static uint8 T8[BLOCK_LEN];
   static uint8 IV8[BLOCK_LEN];

   static uint8 C8[FW_MAX_PCLEN];
   uint32 PLen;

   M_Memcpy( C8, C, CLen );
   M_Memcpy( A8, A, ALen );
   M_Memcpy( T8, T, TLen );
   M_Memcpy( K8, K, KLen );
   M_Memcpy( IV8, IV, IVLen );
   PLen = *PL;

   ErrorStatus = DecryptGCM_Mode( C8, CLen, A8, ALen, T8, TLen, IV8, IVLen, K8, KLen, P8, &PLen );

   *PL = PLen;
   M_Memcpy( P, P8, *PL );

   memset( P8, 0, FW_MAX_PCLEN );
   memset( A8, 0, FW_MAX_ALEN );
   memset( K8, 0, MAX_KEY_LEN );
   memset( IV8, 0, BLOCK_LEN );
   memset( C8, 0, FW_MAX_PCLEN );
   memset( T8, 0, BLOCK_LEN );

   return( ErrorStatus );
}




#if ( RUN_AES_GCM_TEST_VECTORS || RUN_AES_GCM_GALOIS_MULTIPLY_TESTS )
//******************************************************************************
void EncryptGCM_ModeA( uint8 *P, uint32 PLen, uint8 *A, uint32 ALen, uint8 *IV, uint16 IVLen, uint8 *K, uint16 KLen, uint16 WipeKey )
{
   static uint8 P8[FW_MAX_PCLEN];
   static uint8 A8[FW_MAX_ALEN];
   static uint8 K8[MAX_KEY_LEN];
   static uint8 IV8[BLOCK_LEN];

   static uint8 C8[FW_MAX_PCLEN];
   static uint8 T8[BLOCK_LEN];
   uint32 CLen;
   uint16 TLen;

   asc_to_uint8_ptr( P, P8, PLen*2 );
   asc_to_uint8_ptr( A, A8, ALen*2 );
   asc_to_uint8_ptr( K, K8, KLen*2 );
   asc_to_uint8_ptr( IV, IV8, IVLen*2 );

   EncryptGCM_Mode( P8, PLen, A8, ALen, IV8, IVLen, K8, KLen, C8, &CLen, T8, &TLen );

   M_DebugXmitString( "                    Encrypt\r\n" );
   M_DebugPrintData( "P", P8, PLen );
   M_DebugPrintData( "C", C8, CLen );
   M_DebugPrintData( "T", T8, TLen );
   M_DebugXmitString( "\r\n" );
   return;
}

//******************************************************************************
void DecryptGCM_ModeA( uint8 *C, uint32 CLen, uint8 *A, uint32 ALen, uint8 *T, uint16 TLen, uint8 *IV, uint16 IVLen, uint8 *K, uint16 KLen, uint16 WipeKey )
{
   static uint8 P8[FW_MAX_PCLEN];
   static uint8 A8[FW_MAX_ALEN];
   static uint8 K8[MAX_KEY_LEN];
   static uint8 T8[BLOCK_LEN];
   static uint8 IV8[BLOCK_LEN];

   static uint8 C8[FW_MAX_PCLEN];
   uint32 PLen;

   asc_to_uint8_ptr( C, C8, CLen*2 );
   asc_to_uint8_ptr( A, A8, ALen*2 );
   asc_to_uint8_ptr( T, T8, TLen*2 );
   asc_to_uint8_ptr( K, K8, KLen*2 );
   asc_to_uint8_ptr( IV, IV8, IVLen*2 );

   DecryptGCM_Mode( C8, CLen, A8, ALen, T8, TLen, IV8, IVLen, K8, KLen, P8, &PLen );

   M_DebugXmitString( "                    Decrypt\r\n" );
   M_DebugPrintData( "P", P8, PLen );
   M_DebugPrintData( "C", C8, CLen );
   M_DebugPrintData( "T", T8, TLen );
   M_DebugXmitString( "\r\n" );
   return;
}

//******************************************************************************
void AES_GCM_Mode_Test( void )
{
#if RUN_AES_GCM_TEST_VECTORS
   M_DebugXmitString( "***********************************************************************************************************\r\n" );
   M_DebugXmitString( "AES GCM Mode Test Vectors:\r\n" );
   EncryptGCM_ModeA( P_0,  3, A_0,  3, IV_0, 12, K_0, 32, 1 );
   DecryptGCM_ModeA( C_0,  3, A_0,  3, T_0, 16, IV_0, 12, K_0, 32, 1 );

   EncryptGCM_ModeA( P_1,  0, A_1,  0, IV_1, 12, K_1, 16, 1 );
   DecryptGCM_ModeA( C_1,  0, A_1,  0, T_1, 16, IV_1, 12, K_1, 16, 1 );

   EncryptGCM_ModeA( P_2, 16, A_2,  0, IV_2, 12, K_2, 16, 1 );
   DecryptGCM_ModeA( C_2, 16, A_2,  0, T_2, 16, IV_2, 12, K_2, 16, 1 );

   EncryptGCM_ModeA( P_3, 64, A_3,  0, IV_3, 12, K_3, 16, 1 );
   DecryptGCM_ModeA( C_3, 64, A_3,  0, T_3, 16, IV_3, 12, K_3, 16, 1 );

   EncryptGCM_ModeA( P_4, 60, A_4, 20, IV_4, 12, K_4, 16, 1 );
   DecryptGCM_ModeA( C_4, 60, A_4, 20, T_4, 16, IV_4, 12, K_4, 16, 1 );

   EncryptGCM_ModeA( P_13,  0, A_13,  0, IV_13, 12, K_13, 32, 1 );
   DecryptGCM_ModeA( C_13,  0, A_13,  0, T_13, 16, IV_13, 12, K_13, 32, 1 );

   EncryptGCM_ModeA( P_14, 16, A_14,  0, IV_14, 12, K_14, 32, 1 );
   DecryptGCM_ModeA( C_14, 16, A_14,  0, T_14, 16, IV_14, 12, K_14, 32, 1 );

   EncryptGCM_ModeA( P_15, 64, A_15,  0, IV_15, 12, K_15, 32, 1 );
   DecryptGCM_ModeA( C_15, 64, A_15,  0, T_15, 16, IV_15, 12, K_15, 32, 1 );

   EncryptGCM_ModeA( P_16, 60, A_16, 20, IV_16, 12, K_16, 32, 1 );
   DecryptGCM_ModeA( C_16, 60, A_16, 20, T_16, 16, IV_16, 12, K_16, 32, 1 );
   M_DebugXmitString( "***********************************************************************************************************\r\n" );
#endif

#if RUN_AES_GCM_GALOIS_MULTIPLY_TESTS
   MultiplyTest();
#endif
}
#endif

#endif// #if CRYPTO_MATH_AES_GCM
//******************************************************************************
// end of file: AES_GCM_Mode.c

