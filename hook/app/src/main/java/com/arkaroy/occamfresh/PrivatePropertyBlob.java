package com.arkaroy.occamfresh;

/** Original F14 private blob table from pinnedHAL847b1f9e / helper286da8.
 * Pure byte-array decoding, no native calls, values changed or payload dump. */
final class PrivatePropertyBlob {
    private static final int[][] SIZES={
        {168,240,216,16,300,316,160,4,32,4,4,276,76,5832,28,24,8,8,8,8,8,8,8,8,4,1080,4,4,4,80,8,8,2080,4,24,4,112,264,112,128,244,100,8,8,8,8,8,8,8,8,8,264,4,4,4,4,12,60,4236,4236,12,3540,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4},
        {1072,4,4,72,4,4,248864,268,17512,76,268,100,88,20,32,100,640},
        {16400,4,4,20,12,8,4,4,216,168,300,316,276,76,5832,244,196,12032,32,6804,1080,60,8,8},
        {16,16,16,16,16,16,16}
    };
    static final class Result {
        String state="unread";
        long declared;
        int walked,consumed;
        final java.util.Map<Long,String> selected=new java.util.LinkedHashMap<>();
    }
    private static long u32(byte[] b,int at){
        return ((long)b[at]&255)|(((long)b[at+1]&255)<<8)
            |(((long)b[at+2]&255)<<16)|(((long)b[at+3]&255)<<24);
    }
    private static int size(long tag){
        long group=(tag&0x78000000L)>>>28,index=tag&0x87ffffffL;
        if(group<3||group>6||index>=SIZES[(int)group-3].length)return -1;
        return SIZES[(int)group-3][(int)index];
    }
    static Result decode(byte[] bytes){
        Result r=new Result();
        if(bytes==null||bytes.length<4||bytes.length>65536){r.state="invalid_length";return r;}
        r.declared=u32(bytes,0);
        if(r.declared>128){r.state="count_limit";return r;}
        int offset=4;
        for(int i=0;i<r.declared;i++){
            if(offset>bytes.length-4){r.state="truncated_tag";return r;}
            long tag=u32(bytes,offset);offset+=4;int size=size(tag);
            if(size<0){r.state="unknown_tag_"+Long.toHexString(tag);return r;}
            if(size>bytes.length-offset){r.state="truncated_payload";return r;}
            if(tag==0x3000001aL||tag==0x50000006L||tag==0x30000019L||tag==0x50000000L){
                if(r.selected.containsKey(tag)){r.state="duplicate_selected_tag";return r;}
                StringBuilder value=new StringBuilder("bytes=").append(size).append(" firstWords=");
                for(int j=0;j<Math.min(size/4,4);j++)value.append(j==0?"":",").append(u32(bytes,offset+j*4));
                r.selected.put(tag,value.toString());
            }
            offset+=size;r.walked++;
        }
        r.consumed=offset;r.state=offset==bytes.length?"complete":"complete_with_trailing_bytes";return r;
    }
}
