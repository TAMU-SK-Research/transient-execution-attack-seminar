import sys
import os
import argparse
import logging
import shutil
import threading
import subprocess
from pathlib import Path
from rich import traceback
from rich import print
from rich.logging import RichHandler
from rich.progress import track
from rich import print
from rich.live import Live
from rich.table import Table
from typing import List
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import concurrent.futures
from collections import OrderedDict

progName='ERADICATE-CLI'

#################################################################
# Data class for each throughput simulation
#################################################################
from dataclasses import dataclass
@dataclass
class Params:
    # FIXME: Make initialization of Param consistent
    ID : int
    meshRows : int
    flushType : str
    netRoutingType : str
    bench : str
    flushNum : int
    outdir: str
    statFile: str = None
    gem5CMD: str = None
    simCycles : int = None
    status: str = 'WAITING'

class EradicateCLI:
    def __init__(self, args):

        self.cwd                = os.getcwd()
        self.opt                = args.opt
        self.benchmarks         = args.benchmarks
        self.flushTypeList      = args.flush_type
        self.flushNumList       = args.flush_num
        self.netRoutingTypes    = args.network_routing_type

        self.meshRowsList       = args.mesh_rows
        self.outdirRoot         = f'{args.outdir_root}'
        self.csvFile            = f'{self.outdirRoot}/output.csv'

        self.maxWorkers         = args.max_workers
        self.totalTests         = len(self.meshRowsList) \
                                * len(self.flushTypeList) \
                                * len(self.netRoutingTypes) \
                                * len(self.benchmarks)  \
                                * len(self.flushNumList)
        self.normalizeTo        = 'clflush-unicast'
        self.microBenchRoot     = 'micro-bench'

        self.lock               = threading.Lock()

    def buildBench(self):
        # TODO: make Makefile for compilation
        ass_cmd = f'gcc -I{self.cwd}/include \
                -o {self.cwd}/{self.microBenchRoot}/m5op.o \
                -c {self.cwd}/{self.microBenchRoot}/m5op.S'
        lib_cmd = f'ar -rc \
                {self.cwd}/{self.microBenchRoot}/libm5eradicate.a \
                {self.cwd}/{self.microBenchRoot}/m5op.o'
        os.system(ass_cmd)
        os.system(lib_cmd)


        for bench in self.benchmarks:
            compile_cmd = f'gcc -std=c99 --static -I{self.cwd}/include \
                        -o {self.cwd}/{self.microBenchRoot}/{bench} \
                        {self.cwd}/{self.microBenchRoot}/{bench}.c \
                        {self.cwd}/{self.microBenchRoot}/libm5eradicate.a'
            if bench == 'sstate':
                compile_cmd += ' -pthread'

            os.system(compile_cmd)

    def simBench(self):
        if os.path.exists(self.outdirRoot):
            print(f"""[yellow]outdir {self.outdirRoot} exists.
            Please rename or delete it before.""")
            exit(1)

        self.df = self.getNewDataframe()

        finishedTests = 0
        with Live(self.generateTable(finishedTests, -1),
                          refresh_per_second=4) as live:
            with concurrent.futures.ThreadPoolExecutor(
                    max_workers=self.maxWorkers) as executor:
                future_to_params = {
                    executor.submit(
                        self.runBench, params)
                    : params for params in self.forEachTest()
                }

                for future in concurrent.futures.as_completed(
                        future_to_params):
                    params = future_to_params[future]
                    finishedTests += 1
                    live.update(self.generateTable(finishedTests, -2))

        self.df.to_csv(f'{self.csvFile}', index=False)

    def plot(self):
        assert(os.path.exists(self.outdirRoot))
        if os.path.exists(self.csvFile):
            reloadStats = False
            print(f'Reading data from {self.csvFile}')
            self.df = pd.read_csv(self.csvFile)
        else:
            reloadStats = True
            self.df = self.getNewDataframe()

        if reloadStats:
            for params in self.forEachTest():
                print(f'Processing {params.statFile} ...')
                params = self.getStats(params)
                if params.simCycles > 0.0:
                    params.status = '[green]SUCCESS'
                else:
                    params.status = '[red]FAILED'
                self.updateDataframe(params)
            self.df.to_csv(f'{self.csvFile}', index=False)

        print(self.df)
        for meshRows in self.meshRowsList:
            for bench in self.benchmarks:
                data = OrderedDict()
                for flushType in self.flushTypeList:
                    for netRoutingType in self.netRoutingTypes:
                        values = []
                        for flushNum in self.flushNumList:
                            row = self.df.loc[
                                (self.df["MESH ROWS"] == meshRows) &
                                (self.df["BENCH"] == bench) &
                                (self.df["FLUSH TYPE"] == flushType) &
                                (self.df["NET. ROUTING TYPE"] == netRoutingType) &
                                (self.df["FLUSH NUM"] == flushNum)]
                            values.append(row['SIM CYCLES'].values[0])
                        label = f'{flushType}-{netRoutingType}'
                        data[label] = values

                norm_data = OrderedDict()
                for key in data.keys():
                    norm_data[key] = []
                    for i, val in enumerate(data[key]):
                        norm_data[key].append(data[key][i]/data[self.normalizeTo][i])

                df = pd.DataFrame(norm_data, self.flushNumList)
                print(bench)
                print(pd.DataFrame(data, self.flushNumList))
                print(df)
                df.plot(kind='bar', figsize=(15,9))
                plt.legend()
                #plt.legend(loc='lower center', bbox_to_anchor=(0.5, -0.15), ncol=4)
                plt.title("Total Program Execution Time (Normalized to clflush base)", fontsize=18)
                plt.xlabel("Number of cache lines to flush", fontsize=15)
                plt.xticks(rotation=0, ha='right')
                plt.ylabel("Execution Time", fontsize=15)
                plt.savefig(f'{self.outdirRoot}/{bench}-{meshRows}x{meshRows}.png')

    def getStats(self, params):
        simCycles = 0
        with open(params.statFile, "r") as f:
            for line in f:
                if 'simTicks' in line:
                    line = " ".join(line.strip().split())
                    simCycles = float(line.split(' ')[1]) / 500.0
                    params.simCycles = simCycles

        return params

    def runBench(self, params):

        os.system(f'mkdir -p {params.outdir}')
        if not os.path.exists(params.outdir):
            print(f'Failed Creating {params.outdir}')
            exit(1)
        f = open(f'{params.outdir}/cmd', 'w')
        f.write(params.gem5CMD)
        f.close()
        params.status = '[yellow]Running'
        self.updateDataframe(params)
        os.system(params.gem5CMD)
        params.status = '[cyan]READING STATS'
        self.updateDataframe(params)

        params = self.getStats(params)
        if params.simCycles > 0:
            params.status = '[green]SUCCESS'
        else:
            params.status = '[red]FAILED'
        self.updateDataframe(params)

    def forEachTest(self):
        ID = 0
        for meshRows in self.meshRowsList:
            for flushType in self.flushTypeList:
                for netRoutingType in self.netRoutingTypes:
                    for bench in self.benchmarks:
                        for flushNum in self.flushNumList:
                            outdir = f'{self.outdirRoot}/{meshRows}x{meshRows}/{flushType}/{netRoutingType}/{bench}/{flushNum}'
                            p =  Params(
                                ID = ID,
                                meshRows=meshRows,
                                flushType=flushType,
                                netRoutingType=netRoutingType,
                                bench=bench,
                                flushNum=flushNum,
                                outdir=outdir)
                            self.makeGem5CMD(p)
                            ID += 1
                            yield p

    def makeGem5CMD(self, p):

        ncpu = p.meshRows * p.meshRows
        cmd = f'{self.cwd}/build/X86_MESI_Two_Level/gem5.{self.opt} \\\n'
        cmd += f'--outdir={p.outdir} \\\n'
        cmd += f'{self.cwd}/configs/example/se.py \\\n'
        cmd += f'--cpu-type=DerivO3CPU \\\n'
        cmd += f'--num-l2caches={ncpu} \\\n'
        cmd += f'--num-cpus={ncpu} \\\n'
        cmd += f'--num-dirs={ncpu} \\\n'
        cmd += f'--ruby \\\n'
        cmd += f'--network=garnet \\\n'
        cmd += f'--topology=Mesh_XY \\\n'
        cmd += f'--mesh-rows={p.meshRows} \\\n'

        cmd += f'--router-latency=1 \\\n'
        cmd += f'--link-latency=1 \\\n'
        cmd += f'--vcs-per-vnet=4 \\\n'
        cmd += f'--link-width-bits=128 \\\n'
        cmd += f'--garnet-deadlock-threshold=10000000 \\\n'
        cmd += f'--trace-packet-id=0 \\\n'
        cmd += f'--cmd={self.cwd}/{self.microBenchRoot}/{p.bench} \\\n'

        if p.flushType == 'clflush':
            cmd += f'--options="{p.flushNum} 1 {ncpu-1}" \\\n'
        else:
            cmd += f'--options="{p.flushNum} 0 {ncpu-1}" \\\n'

        if p.netRoutingType == 'multicast':
            cmd += f'--enable-rpm \\\n'
            cmd += f'--routing-algorithm=2 \\\n'
        else:
            cmd += f'--routing-algorithm=1 \\\n'
        cmd += f'> {p.outdir}/log 2>&1\n'

        p.gem5CMD= cmd
        p.statFile = f'{p.outdir}/stats.txt'

    def generateTable(self, finishedTests, id) -> Table:
        self.lock.acquire()
        """Make a new Table"""
        table = Table()
        table.add_column('MESH ROWS'        )
        table.add_column('FLUSH TYPE'       )
        table.add_column('NET. ROUTING TYPE')
        table.add_column('BENCH'            )
        table.add_column('FLUSH NUM'        )
        table.add_column('STATUS'           )

        progress = (finishedTests/float(self.totalTests))
        table.title = f'[not italic]:popcorn:[/]Eradicate Simulation '
        table.title += f'{progress:.0%} ({finishedTests}/{self.totalTests})'
        table.title += f'[not italic]:popcorn:[/]'


        for index, row in self.df.iterrows():
            if 'SUCCESS' not in row['STATUS']:
                table.add_row(
                    f"{row['MESH ROWS'        ]}",
                    f"{row['FLUSH TYPE'       ]}",
                    f"{row['NET. ROUTING TYPE']}",
                    f"{row['BENCH'            ]}",
                    f"{row['FLUSH NUM'        ]}",
                    f"{row['STATUS'           ]}",
            )
        self.lock.release()
        return table

    def updateDataframe(self, params):

        self.lock.acquire()

        if len(self.df.loc[
            (self.df['MESH ROWS'] == params.meshRows)
            & (self.df['FLUSH TYPE'] == params.flushType)
            & (self.df['NET. ROUTING TYPE'] == params.netRoutingType)
            & (self.df['BENCH'] == params.bench)
            & (self.df['FLUSH NUM'] == params.flushNum)]) == 0:
            self.df = self.df.append(
                self.getNewDataframeRow(params),
                ignore_index=True
            )
        else:
            self.df.loc[(self.df['MESH ROWS'] == params.meshRows)
                & (self.df['FLUSH TYPE'] == params.flushType)
                & (self.df['NET. ROUTING TYPE'] == params.netRoutingType)
                & (self.df['BENCH'] == params.bench)
                & (self.df['FLUSH NUM'] == params.flushNum)] \
            = [
                params.meshRows,
                params.flushType,
                params.netRoutingType,
                params.bench,
                params.flushNum,
                params.simCycles,
                params.status,
            ]

        self.lock.release()

    def getNewDataframeRow(self, params):
        return {
            'MESH ROWS'         : params.meshRows,
            'FLUSH TYPE'        : params.flushType,
            'NET. ROUTING TYPE' : params.netRoutingType,
            'BENCH'             : params.bench,
            'FLUSH NUM'         : params.flushNum,
            'SIM CYCLES'        : params.simCycles,
            'STATUS'            : params.status,
        }
    def getNewDataframe(self):
        return pd.DataFrame(columns=[
            'MESH ROWS',
            'FLUSH TYPE',
            'NET. ROUTING TYPE',
            'BENCH',
            'FLUSH NUM',
            'SIM CYCLES',
            'STATUS',
        ])

#################################################################
# Sub commands
#################################################################
def buildGEM5(args):
    cmd = f'scons {os.getcwd()}/build/X86_MESI_Two_Level/gem5.{args.opt} -j16'
    os.system(cmd)

def buildBench(args):
    eradicateCLI = EradicateCLI(args)
    eradicateCLI.buildBench()

def runBench(args):
    eradicateCLI = EradicateCLI(args)
    eradicateCLI.buildBench()
    eradicateCLI.simBench()
    eradicateCLI.plot()

def plot(args):
    eradicateCLI = EradicateCLI(args)
    eradicateCLI.plot()

def main(argv: List[str] = None):
    parser = argparse.ArgumentParser(
        prog=progName,
        description="ERADICATE Commandline Interface",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )
    parser.set_defaults(func=lambda x: parser.print_help())
    #################################################################
    # Common arguments
    #################################################################
    parser.add_argument('--opt', action='store',
                            type=str, default='debug',
                            help="Build options (debug/opt/fast).")

    parser.add_argument('--benchmarks', action='store',
                            type=str, nargs='+',
                            choices=['npstate', 'estate', 'mstate', 'sstate'],
                            default=['npstate', 'estate', 'mstate', 'sstate'],
                            help="List of benchmarks to simulate")

    parser.add_argument('--flush-type', action='store',
                            type=str, nargs='+',
                            choices=['clflush', 'eradicate'],
                            default=['clflush', 'eradicate'],
                            help="Method to flush cache lines")

    parser.add_argument('--flush-num', action='store',
                            type=int, nargs='+', default=[1024, 2048, 4096, 8192],
                            help="List of flush numsk.")

    parser.add_argument('--network-routing-type', action='store',
                            type=str, nargs='+',
                            choices=['unicast', 'multicast'],
                            default=['unicast', 'multicast'],
                            help="Method to routing multicast packet.")

    parser.add_argument('--mesh-rows', action='store',
                            type=int, nargs='+', default=[4,8],
                            help="List of mesh rows to simulate.")

    parser.add_argument('--outdir-root', action='store',
                            type=str, default='./m5out', help="",)

    parser.add_argument('--max-workers',
                            type=int, default=16, help="",)

    subparsers = parser.add_subparsers(title='commands')
    #################################################################
    # Sub command - build gem5
    #################################################################
    parser_buildGEM5 = subparsers.add_parser('build-gem5', help='Make gem5 executable.')
    parser_buildGEM5.set_defaults(func=buildGEM5)

    #################################################################
    # Sub command - build benchmarks
    #################################################################
    parser_buildBench = subparsers.add_parser('build-bench', help='Make benchmarks executables.')
    parser_buildBench.set_defaults(func=buildBench)

    #################################################################
    # Sub command - sim-throughput
    #################################################################
    parser_runBench= subparsers.add_parser('run-bench',
                                help='Start garnet throughput simulation')
    parser_runBench.set_defaults(func=runBench)

    #################################################################
    # Sub command - plot
    #################################################################
    parser_plot = subparsers.add_parser('plot', help='Make figures.')
    parser_plot.set_defaults(func=plot)

    #################################################################
    # Create CLI class and call selected sub command.
    #################################################################
    try:
        args = parser.parse_args(sys.argv[1:])
        args.func(args)
    except KeyboardInterrupt:
        print(f"{argv}{os.linesep}")
        sys.exit(0)

if __name__ == "__main__":
    sys.exit(main())
