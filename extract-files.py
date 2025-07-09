#!/usr/bin/env -S PYTHONPATH=../../../tools/extract-utils python3
#
# SPDX-FileCopyrightText: 2024 The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

from extract_utils.file import File
from extract_utils.fixups_blob import (
    BlobFixupCtx,
    blob_fixup,
    blob_fixups_user_type,
)
from extract_utils.fixups_lib import (
    lib_fixup_remove,
    lib_fixups,
    lib_fixups_user_type,
)
from extract_utils.main import (
    ExtractUtils,
    ExtractUtilsModule,
)

namespace_imports = [
    'device/fairphone/FP6',
    'hardware/qcom-caf/sm8650',
    'hardware/qcom-caf/wlan',
    'vendor/qcom/opensource/commonsys/display',
    'vendor/qcom/opensource/commonsys-intf/display',
    'vendor/qcom/opensource/dataservices',
]


def lib_fixup_vendor_suffix(lib: str, partition: str, *args, **kwargs):
    return f'{lib}_{partition}' if partition == 'vendor' else None


lib_fixups: lib_fixups_user_type = {
    **lib_fixups,
    (
        'com.qualcomm.qti.dpm.api@1.0',
        'vendor.qti.diaghal@1.0',
        'vendor.qti.hardware.dpmaidlservice-V1-ndk',
        'vendor.qti.hardware.qccsyshal@1.0',
        'vendor.qti.hardware.qccsyshal@1.1',
        'vendor.qti.hardware.qccsyshal@1.2',
        'vendor.qti.hardware.wifidisplaysession@1.0',
        'vendor.qti.ImsRtpService-V1-ndk',
        'vendor.qti.imsrtpservice@3.0',
        'vendor.qti.imsrtpservice@3.1',
        'vendor.qti.qccvndhal_aidl-V1-ndk',
    ): lib_fixup_vendor_suffix,
    (
        'libagm',
        'libar-acdb',
        'libar-pal',
        'libats',
        'liblx-osal',
        'libpalclient',
    ): lib_fixup_remove,
}


def blob_fixup_sql_reorder(
    ctx: BlobFixupCtx, file: File, file_path: str, *args, **kwargs
):
    # fairphone tries to insert before creating the table, leading to build errors
    # this moves the create table up before the first insert statement
    create_stmt = 'CREATE TABLE qcril_emergency_source_voice_mcc_mnc_table'
    insert_stmt = 'INSERT INTO qcril_emergency_source_voice_mcc_mnc_table'

    with open(file_path, 'rb+') as f:
        text = f.read().decode('utf-8')
        lines = text.splitlines()

        # Remove the table creation
        create_line = None
        for i, line in enumerate(lines):
            if line.strip().startswith(create_stmt):
                create_line = lines.pop(i)
                break

        # Add the table creation before the first insert
        if create_line:
            for i, line in enumerate(lines):
                if line.strip().startswith(insert_stmt):
                    lines.insert(i, create_line)
                    break

        f.seek(0)
        f.write(('\n'.join(lines) + '\n').encode('utf-8'))
        f.truncate()


blob_fixups: blob_fixups_user_type = {
    (
        'vendor/bin/qcc-vendor',
        'vendor/bin/qms',
        'vendor/bin/xtra-daemon',
        'vendor/lib64/libcne.so',
        'vendor/lib64/libqcc_sdk.so',
        'vendor/lib64/libqms_client.so',
    ): blob_fixup()
        .add_needed('libbinder_shim.so'),
    'vendor/etc/init/tctd.rc': blob_fixup()
        .regex_replace('.+seclabel.+\n', ''),
    'vendor/etc/seccomp_policy/wfdhdcphalservice.policy': blob_fixup()
        .add_line_if_missing('rt_tgsigqueueinfo: 1'),
    'vendor/etc/qcril_database/upgrade/other/0000_initial_qcrilnr.sql': blob_fixup().
        call(blob_fixup_sql_reorder),
    (
        'vendor/lib64/camera/com.qti.eeprom.gt24p128c2csli_imx766.so',
        'vendor/lib64/camera/com.qti.eeprom.gt24p64b_imx688.so',
        'vendor/lib64/camera/com.qti.eeprom.irs2381c_polar.so',
        'vendor/lib64/camera/com.qti.eeprom.truly_cmb433.so',
        'vendor/lib64/camera/com.qti.ois.bu63169gwz.so',
        'vendor/lib64/camera/com.qti.ois.dw9784.so',
        'vendor/lib64/camera/com.qti.sensor.camano_imx688.so',
        'vendor/lib64/camera/com.qti.sensor.camano_imx766.so',
        'vendor/lib64/camera/com.qti.sensor.camano_ov64b40.so',
        'vendor/lib64/camera/com.qti.sensor.camano_s5k3m5.so',
        'vendor/lib64/camera/com.qti.sensor.fp6_imx896.so',
        'vendor/lib64/camera/com.qti.sensor.imx362.so',
        'vendor/lib64/camera/com.qti.sensor.imx363.so',
        'vendor/lib64/camera/com.qti.sensor.imx376.so',
        'vendor/lib64/camera/com.qti.sensor.imx386.so',
        'vendor/lib64/camera/com.qti.sensor.imx476.so',
        'vendor/lib64/camera/com.qti.sensor.imx481.so',
        'vendor/lib64/camera/com.qti.sensor.imx519.so',
        'vendor/lib64/camera/com.qti.sensor.imx563.so',
        'vendor/lib64/camera/com.qti.sensor.imx576.so',
        'vendor/lib64/camera/com.qti.sensor.imx586.so',
        'vendor/lib64/camera/com.qti.sensor.imx588.so',
        'vendor/lib64/camera/com.qti.sensor.imx686.so',
        'vendor/lib64/camera/com.qti.sensor.imx688.so',
        'vendor/lib64/camera/com.qti.sensor.imx766.so',
        'vendor/lib64/camera/com.qti.sensor.max7366_6dof.so',
        'vendor/lib64/camera/com.qti.sensor.max7366_eyetrack.so',
        'vendor/lib64/camera/com.qti.sensor.max7366_ov6211.so',
        'vendor/lib64/camera/com.qti.sensor.max7366_ov9282.so',
        'vendor/lib64/camera/com.qti.sensor.ov13855.so',
        'vendor/lib64/camera/com.qti.sensor.ov13b10.so',
        'vendor/lib64/camera/com.qti.sensor.ov64b40.so',
        'vendor/lib64/camera/com.qti.sensor.ov7251.so',
        'vendor/lib64/camera/com.qti.sensor.s5k2l7.so',
        'vendor/lib64/camera/com.qti.sensor.s5k2x5sp.so',
        'vendor/lib64/camera/com.qti.sensor.s5k33dxx.so',
        'vendor/lib64/camera/com.qti.sensor.s5k3m5.so',
        'vendor/lib64/camera/com.qti.sensor.s5k3p9.so',
        'vendor/lib64/camera/com.qti.sensor.s5k4h7.so',
        'vendor/lib64/camera/com.qti.sensor.s5k5e9yu05.so',
        'vendor/lib64/camera/com.qti.sensor.s5k5e9yx04.so',
        'vendor/lib64/camera/com.qti.sensor.s5khp1s.so',
        'vendor/lib64/camera/com.qti.sensor.s5kjd1sp.so',
        'vendor/lib64/camera/com.qti.sensor.s5kkd1sp.so',
        'vendor/lib64/camera/components/com.qti.node.depth.so',
        'vendor/lib64/camera/components/com.qti.node.depthprovider.so',
        'vendor/lib64/camera/components/com.qti.node.dewarp.so',
        'vendor/lib64/camera/components/com.qti.node.eisv2.so',
        'vendor/lib64/camera/components/com.qti.node.eisv3.so',
        'vendor/lib64/camera/components/com.qti.node.evadepth.so',
        'vendor/lib64/camera/components/com.qti.node.gme.so',
        'vendor/lib64/camera/components/com.qti.node.gyrornn.so',
        'vendor/lib64/camera/components/com.qti.node.hdr10pgen.so',
        'vendor/lib64/camera/components/com.qti.node.hdr10phist.so',
        'vendor/lib64/camera/components/com.qti.node.itofpreprocess.so',
        'vendor/lib64/camera/components/com.qti.node.ml.so',
        'vendor/lib64/camera/components/com.qti.node.seg.so',
        'vendor/lib64/camera/components/com.qti.node.swec.so',
        'vendor/lib64/camera/components/com.qti.node.swregistration.so',
        'vendor/lib64/camera/components/com.qti.stats.cnndriver.so',
        'vendor/lib64/camera/components/libdepthmapwrapper_itof.so',
        'vendor/lib64/camera/components/libdepthmapwrapper_secure.so',
        'vendor/lib64/com.qti.camx.chiiqutils.so',
        'vendor/lib64/com.qti.chiusecaseselector.so',
        'vendor/lib64/com.qti.feature2.afbrckt.so',
        'vendor/lib64/com.qti.feature2.anchorsync.so',
        'vendor/lib64/com.qti.feature2.demux.so',
        'vendor/lib64/com.qti.feature2.derivedoffline.so',
        'vendor/lib64/com.qti.feature2.fusion.so',
        'vendor/lib64/com.qti.feature2.generic.so',
        'vendor/lib64/com.qti.feature2.gs.milos.so',
        'vendor/lib64/com.qti.feature2.hdr.so',
        'vendor/lib64/com.qti.feature2.mcreprocrt.so',
        'vendor/lib64/com.qti.feature2.memcpy.so',
        'vendor/lib64/com.qti.feature2.metadataserializer.so',
        'vendor/lib64/com.qti.feature2.mfsr.milos.so',
        'vendor/lib64/com.qti.feature2.mfsr.so',
        'vendor/lib64/com.qti.feature2.ml.so',
        'vendor/lib64/com.qti.feature2.mux.so',
        'vendor/lib64/com.qti.feature2.qcfa.so',
        'vendor/lib64/com.qti.feature2.rawhdr.so',
        'vendor/lib64/com.qti.feature2.realtimeserializer.so',
        'vendor/lib64/com.qti.feature2.rt.so',
        'vendor/lib64/com.qti.feature2.rtmcx.so',
        'vendor/lib64/com.qti.feature2.serializer.so',
        'vendor/lib64/com.qti.feature2.statsregeneration.so',
        'vendor/lib64/com.qti.feature2.stub.so',
        'vendor/lib64/com.qti.feature2.swmf.so',
        'vendor/lib64/com.qti.qseeutils.so',
        'vendor/lib64/com.qualcomm.mcx.distortionmapper.so',
        'vendor/lib64/com.qualcomm.mcx.linearmapper.so',
        'vendor/lib64/com.qualcomm.mcx.nonlinearmapper.so',
        'vendor/lib64/com.qualcomm.mcx.policy.mfl.so',
        'vendor/lib64/com.qualcomm.qti.mcx.usecase.extension.so',
        'vendor/lib64/hw/camera.qcom.milos.so',
        'vendor/lib64/hw/camera.qcom.so',
        'vendor/lib64/hw/com.qti.chi.offline.so',
        'vendor/lib64/hw/com.qti.chi.override.so',
        'vendor/lib64/libcamerapostproc.so',
        'vendor/lib64/libcamxhwnodecontext.so',
        'vendor/lib64/libcamxifestriping.so',
        'vendor/lib64/libcamximageformatutils.so',
        'vendor/lib64/libcamxncsdatafactory.so',
        'vendor/lib64/libchifeature2.so',
        'vendor/lib64/libcommonchiutils.so',
        'vendor/lib64/libhme.so',
        'vendor/lib64/libipebpsstriping.so',
        'vendor/lib64/libipebpsstriping170.so',
        'vendor/lib64/libipebpsstriping480.so',
        'vendor/lib64/libisphwsetting.so',
        'vendor/lib64/libjpege.so',
        'vendor/lib64/libmctfengine_stub.so',
        'vendor/lib64/libmfec.so',
        'vendor/lib64/libmmcamera_bestats.so',
        'vendor/lib64/libmmcamera_cac.so',
        'vendor/lib64/libmmcamera_lscv35.so',
        'vendor/lib64/libmmcamera_mfnr.so',
        'vendor/lib64/libmmcamera_mfnr_t4.so',
        'vendor/lib64/libmmcamera_pdpc.so',
        'vendor/lib64/libopestriping.so',
        'vendor/lib64/libtfestriping.so',
        'vendor/lib64/libubifocus.so',
        'vendor/lib64/vendor.qti.hardware.camera.aon-service-impl.so',
        'vendor/lib64/vendor.qti.hardware.camera.offlinecamera-service-impl.so',
        'vendor/lib64/vendor.qti.hardware.camera.postproc@1.0-service-impl.so',
    ): blob_fixup().replace_needed(
        'android.hardware.graphics.allocator-V1-ndk.so',
        'android.hardware.graphics.allocator-V2-ndk.so',
    ),
    (
        'vendor/lib64/libmfnr_raw_api.so',
        'vendor/lib64/libTclAISuperfine.so',
        'vendor/lib64/libTclImage_ImageEngine.so',
    ): blob_fixup()
        .clear_symbol_version('AHardwareBuffer_allocate')
        .clear_symbol_version('AHardwareBuffer_describe')
        .clear_symbol_version('AHardwareBuffer_lock')
        .clear_symbol_version('AHardwareBuffer_release')
        .clear_symbol_version('AHardwareBuffer_unlock'),
    'vendor/lib64/libqcodec2_core.so': blob_fixup()
        .add_needed('libcodec2_shim.so'),
    'vendor/lib64/vendor.libdpmframework.so': blob_fixup()
        .add_needed('libbinder_shim.so')
        .add_needed('libhidlbase_shim.so'),

}  # fmt: skip


module = ExtractUtilsModule(
    'FP6',
    'fairphone',
    blob_fixups=blob_fixups,
    lib_fixups=lib_fixups,
    namespace_imports=namespace_imports,
    add_firmware_proprietary_file=True,
)

if __name__ == '__main__':
    utils = ExtractUtils.device(module)
    utils.run()
