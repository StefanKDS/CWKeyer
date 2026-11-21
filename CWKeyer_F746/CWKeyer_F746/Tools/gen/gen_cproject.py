#!/usr/bin/env python3
"""Generates .project and .cproject for STM32CubeIDE (managed build, no .ioc)."""
import os

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")
NAME = "CWKeyer_F746"
P = "com.st.stm32cube.ide.mcu.gnu.managedbuild"

INCLUDES = ["../Core/Inc", "../App/Inc", "../Gui/Inc", "../Lib/Inc", "../Platform/Inc",
            "../Drivers/STM32F7xx_HAL_Driver/Inc", "../Drivers/STM32F7xx_HAL_Driver/Inc/Legacy",
            "../Drivers/CMSIS/Device/ST/STM32F7xx/Include", "../Drivers/CMSIS/Include",
            "../Drivers/BSP/STM32746G-Discovery", "../Drivers/BSP/Components/wm8994",
            "../Drivers/BSP/Components/Common"]
SOURCES = ["App", "Core", "Drivers", "Gui", "Lib", "Platform"]

_n = [1000]
def nid():
    _n[0] += 1
    return str(_n[0])

def config(debug):
    name = "Debug" if debug else "Release"
    kind = "debug" if debug else "release"
    cid = f"{P}.config.exe.{kind}.{nid()}"
    opt = "og" if debug else "o2"
    dbg = "g3" if debug else "g0"
    defs = (["DEBUG"] if debug else []) + ["USE_HAL_DRIVER", "STM32F746xx"]
    cc_id = f"{P}.tool.c.compiler.{nid()}"
    cc_in = f"{P}.tool.c.compiler.input.c.{nid()}"
    x = []
    a = x.append
    a(f'\t\t<cconfiguration id="{cid}">')
    a(f'\t\t\t<storageModule buildSystemId="org.eclipse.cdt.managedbuilder.core.configurationDataProvider" id="{cid}" moduleId="org.eclipse.cdt.core.settings" name="{name}">')
    a('\t\t\t\t<externalSettings/>')
    a('\t\t\t\t<extensions>')
    for e, pnt in (("org.eclipse.cdt.core.ELF", "BinaryParser"), ("org.eclipse.cdt.core.GASErrorParser", "ErrorParser"),
                   ("org.eclipse.cdt.core.GLDErrorParser", "ErrorParser"), ("org.eclipse.cdt.core.GCCErrorParser", "ErrorParser")):
        a(f'\t\t\t\t\t<extension id="{e}" point="org.eclipse.cdt.core.{pnt}"/>')
    a('\t\t\t\t</extensions>')
    a('\t\t\t</storageModule>')
    a('\t\t\t<storageModule moduleId="cdtBuildSystem" version="4.0.0">')
    a(f'\t\t\t\t<configuration artifactExtension="elf" artifactName="${{ProjName}}" buildArtefactType="org.eclipse.cdt.build.core.buildArtefactType.exe" '
      f'buildProperties="org.eclipse.cdt.build.core.buildArtefactType=org.eclipse.cdt.build.core.buildArtefactType.exe,org.eclipse.cdt.build.core.buildType=org.eclipse.cdt.build.core.buildType.{kind}" '
      f'cleanCommand="rm -rf" description="" id="{cid}" name="{name}" parent="{P}.config.exe.{kind}">')
    a(f'\t\t\t\t\t<folderInfo id="{cid}." name="/" resourcePath="">')
    a(f'\t\t\t\t\t\t<toolChain id="{P}.toolchain.exe.{kind}.{nid()}" name="MCU ARM GCC" superClass="{P}.toolchain.exe.{kind}">')
    def o(sc, val, vt="string"):
        a(f'\t\t\t\t\t\t\t<option id="{sc}.{nid()}" superClass="{sc}" value="{val}" valueType="{vt}"/>')
    o("com.st.stm32cube.ide.mcu.option.internal.toolchain.type", "com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32")
    o(f"{P}.option.target_mcu", "STM32F746NGHx")
    o(f"{P}.option.target_cpuid", "0")
    o(f"{P}.option.target_coreid", "0")
    o(f"{P}.option.fpu", f"{P}.option.fpu.value.fpv5-sp-d16", "enumerated")
    o(f"{P}.option.floatabi", f"{P}.option.floatabi.value.hard", "enumerated")
    o(f"{P}.option.target_board", "STM32F746G-DISCO")
    o("com.st.stm32cube.ide.mcu.debug.option.cpuclock", "200")
    a(f'\t\t\t\t\t\t\t<targetPlatform archList="all" binaryParser="org.eclipse.cdt.core.ELF" id="{P}.targetplatform.{nid()}" isAbstract="false" osList="all" superClass="{P}.targetplatform"/>')
    a(f'\t\t\t\t\t\t\t<builder buildPath="${{workspace_loc:/{NAME}}}/{name}" id="{P}.builder.{nid()}" keepEnvironmentInBuildfile="false" managedBuildOn="true" name="Gnu Make Builder" parallelBuildOn="true" parallelizationNumber="optimal" superClass="{P}.builder"/>')
    # assembler
    t = f"{P}.tool.assembler"
    a(f'\t\t\t\t\t\t\t<tool id="{t}.{nid()}" name="MCU/MPU GCC Assembler" superClass="{t}">')
    a(f'\t\t\t\t\t\t\t\t<option id="{t}.option.debuglevel.{nid()}" superClass="{t}.option.debuglevel" value="{t}.option.debuglevel.value.{dbg}" valueType="enumerated"/>')
    a(f'\t\t\t\t\t\t\t\t<inputType id="{t}.input.{nid()}" superClass="{t}.input"/>')
    a('\t\t\t\t\t\t\t</tool>')
    # compiler
    t = f"{P}.tool.c.compiler"
    a(f'\t\t\t\t\t\t\t<tool id="{cc_id}" name="MCU/MPU GCC Compiler" superClass="{t}">')
    a(f'\t\t\t\t\t\t\t\t<option id="{t}.option.debuglevel.{nid()}" superClass="{t}.option.debuglevel" value="{t}.option.debuglevel.value.{dbg}" valueType="enumerated"/>')
    a(f'\t\t\t\t\t\t\t\t<option id="{t}.option.optimization.level.{nid()}" superClass="{t}.option.optimization.level" value="{t}.option.optimization.level.value.{opt}" valueType="enumerated"/>')
    a(f'\t\t\t\t\t\t\t\t<option IS_BUILTIN_EMPTY="false" IS_VALUE_EMPTY="false" id="{t}.option.definedsymbols.{nid()}" superClass="{t}.option.definedsymbols" valueType="definedSymbols">')
    for d in defs:
        a(f'\t\t\t\t\t\t\t\t\t<listOptionValue builtIn="false" value="{d}"/>')
    a('\t\t\t\t\t\t\t\t</option>')
    a(f'\t\t\t\t\t\t\t\t<option IS_BUILTIN_EMPTY="false" IS_VALUE_EMPTY="false" id="{t}.option.includepaths.{nid()}" superClass="{t}.option.includepaths" valueType="includePath">')
    for i in INCLUDES:
        a(f'\t\t\t\t\t\t\t\t\t<listOptionValue builtIn="false" value="{i}"/>')
    a('\t\t\t\t\t\t\t\t</option>')
    a(f'\t\t\t\t\t\t\t\t<inputType id="{cc_in}" superClass="{t}.input.c"/>')
    a('\t\t\t\t\t\t\t</tool>')
    # c++ compiler (unused, but expected by the tool chain)
    t = f"{P}.tool.cpp.compiler"
    a(f'\t\t\t\t\t\t\t<tool id="{t}.{nid()}" name="MCU/MPU G++ Compiler" superClass="{t}"/>')
    # linker
    t = f"{P}.tool.c.linker"
    a(f'\t\t\t\t\t\t\t<tool id="{t}.{nid()}" name="MCU/MPU GCC Linker" superClass="{t}">')
    a(f'\t\t\t\t\t\t\t\t<option id="{t}.option.script.{nid()}" superClass="{t}.option.script" value="${{workspace_loc:/${{ProjName}}/STM32F746NGHX_FLASH.ld}}" valueType="string"/>')
    a(f'\t\t\t\t\t\t\t\t<inputType id="{t}.input.{nid()}" superClass="{t}.input">')
    a('\t\t\t\t\t\t\t\t\t<additionalInput kind="additionalinputdependency" paths="$(USER_OBJS)"/>')
    a('\t\t\t\t\t\t\t\t\t<additionalInput kind="additionalinput" paths="$(LIBS)"/>')
    a('\t\t\t\t\t\t\t\t</inputType>')
    a('\t\t\t\t\t\t\t</tool>')
    for t, n in (("cpp.linker", "MCU/MPU G++ Linker"), ("archiver", "MCU/MPU GCC Archiver"), ("size", "MCU Size"),
                 ("objdump.listfile", "MCU Output Converter list file"), ("objcopy.hex", "MCU Output Converter Hex"),
                 ("objcopy.binary", "MCU Output Converter Binary"), ("objcopy.verilog", "MCU Output Converter Verilog"),
                 ("objcopy.srec", "MCU Output Converter Motorola S-rec"), ("objcopy.symbolsrec", "MCU Output Converter Motorola S-rec with symbols")):
        a(f'\t\t\t\t\t\t\t<tool id="{P}.tool.{t}.{nid()}" name="{n}" superClass="{P}.tool.{t}"/>')
    a('\t\t\t\t\t\t</toolChain>')
    a('\t\t\t\t\t</folderInfo>')
    a('\t\t\t\t\t<sourceEntries>')
    for s in SOURCES:
        a(f'\t\t\t\t\t\t<entry flags="VALUE_WORKSPACE_PATH|RESOLVED" kind="sourcePath" name="{s}"/>')
    a('\t\t\t\t\t</sourceEntries>')
    a('\t\t\t\t</configuration>')
    a('\t\t\t</storageModule>')
    a('\t\t\t<storageModule moduleId="org.eclipse.cdt.core.externalSettings"/>')
    a('\t\t</cconfiguration>')
    return x, cid, cc_id, cc_in


def main():
    d, d_id, d_cc, d_in = config(True)
    r, r_id, r_cc, r_in = config(False)
    x = ['<?xml version="1.0" encoding="UTF-8" standalone="no"?>',
         '<?fileVersion 4.0.0?><cproject storage_type_id="org.eclipse.cdt.core.XmlProjectDescriptionStorage">',
         '\t<storageModule moduleId="org.eclipse.cdt.core.settings">'] + d + r + ['\t</storageModule>',
         '\t<storageModule moduleId="cdtBuildSystem" version="4.0.0">',
         f'\t\t<project id="{NAME}.null.{nid()}" name="{NAME}"/>',
         '\t</storageModule>',
         '\t<storageModule moduleId="scannerConfiguration">',
         '\t\t<autodiscovery enabled="true" problemReportingEnabled="true" selectedProfileId=""/>',
         f'\t\t<scannerConfigBuildInfo instanceId="{d_id};{d_id}.;{d_cc};{d_in}">',
         '\t\t\t<autodiscovery enabled="false" problemReportingEnabled="true" selectedProfileId=""/>',
         '\t\t</scannerConfigBuildInfo>',
         f'\t\t<scannerConfigBuildInfo instanceId="{r_id};{r_id}.;{r_cc};{r_in}">',
         '\t\t\t<autodiscovery enabled="false" problemReportingEnabled="true" selectedProfileId=""/>',
         '\t\t</scannerConfigBuildInfo>',
         '\t</storageModule>',
         '\t<storageModule moduleId="org.eclipse.cdt.core.LanguageSettingsProviders"/>',
         '\t<storageModule moduleId="refreshScope" versionNumber="2">',
         '\t\t<configuration configurationName="Debug">',
         f'\t\t\t<resource resourceType="PROJECT" workspacePath="/{NAME}"/>',
         '\t\t</configuration>',
         '\t\t<configuration configurationName="Release">',
         f'\t\t\t<resource resourceType="PROJECT" workspacePath="/{NAME}"/>',
         '\t\t</configuration>',
         '\t</storageModule>',
         '\t<storageModule moduleId="com.st.stm32cube.ide.mcu.sfrview"/>',
         '\t<storageModule moduleId="com.st.stm32cube.ide.mcu.rtosview"/>',
         '</cproject>', '']
    open(os.path.join(ROOT, ".cproject"), "w", newline="\n").write("\n".join(x))

    proj = f'''<?xml version="1.0" encoding="UTF-8"?>
<projectDescription>
	<name>{NAME}</name>
	<comment>CW Keyer for STM32F746G-DISCO</comment>
	<projects>
	</projects>
	<buildSpec>
		<buildCommand>
			<name>org.eclipse.cdt.managedbuilder.core.genmakebuilder</name>
			<triggers>clean,full,incremental,</triggers>
			<arguments>
			</arguments>
		</buildCommand>
		<buildCommand>
			<name>org.eclipse.cdt.managedbuilder.core.ScannerConfigBuilder</name>
			<triggers>full,incremental,</triggers>
			<arguments>
			</arguments>
		</buildCommand>
	</buildSpec>
	<natures>
		<nature>com.st.stm32cube.ide.mcu.MCUProjectNature</nature>
		<nature>org.eclipse.cdt.core.cnature</nature>
		<nature>com.st.stm32cube.ide.mcu.MCUCubeIdeServicesRevAev2ProjectNature</nature>
		<nature>com.st.stm32cube.ide.mcu.MCUSingleCpuProjectNature</nature>
		<nature>com.st.stm32cube.ide.mcu.MCURootProjectNature</nature>
		<nature>org.eclipse.cdt.managedbuilder.core.managedBuildNature</nature>
		<nature>org.eclipse.cdt.managedbuilder.core.ScannerConfigNature</nature>
	</natures>
</projectDescription>
'''
    open(os.path.join(ROOT, ".project"), "w", newline="\n").write(proj)
    print("ok")


if __name__ == "__main__":
    main()
