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

progName='RPM-CLI'

#################################################################
# Data class for each throughput simulation
#################################################################
from dataclasses import dataclass
@dataclass
class Params:
    meshRows: int
    scheme: str
    trafficPattern: str
    rpmPacketType: int
    injRate: float
    effInjRate: float = -1
    outdir: str = None
    statFile: str = None
    gem5CMD: str = None
    latency: float = -1
    avgLinkUtil: float = -1
    totalInjectedPacket: int = 0
    status: str = 'WAITING'

class RPMCLI:
    def __init__(self, args):

        self.opt                = args.opt
        self.cwd                = os.getcwd()
        self.meshRowsList       = args.mesh_rows
        self.schemes            = args.schemes
        self.trafficPatterns    = args.traffic_patterns
        self.rpmPacketTypes     = args.rpm_packet_types
        self.injRates           = [round(x, 3) for x in np.arange(0.005, 1.0, 0.005)]
        self.outdirRoot         = f'{args.outdir_root}'
        self.csvFile            = f'{self.outdirRoot}/output.csv'

        self.max_workers        = args.max_workers
        self.totalTests         = len(self.meshRowsList) \
                                * len(self.schemes) \
                                * len(self.trafficPatterns) \
                                * len(self.rpmPacketTypes) \
                                * len(self.injRates)
        self.lock               = threading.Lock()

    def simThroughput(self):
        if os.path.exists(self.outdirRoot):
            print(f"""[yellow]outdir {self.outdirRoot} exists.
            Please rename or delete it before.""")
            exit(1)

        self.df = self.getNewDataframe()

        finishedTests = 0
        with Live(self.generateTable(finishedTests),
                          refresh_per_second=4) as live:
            with concurrent.futures.ThreadPoolExecutor(
                    max_workers=self.max_workers) as executor:
                future_to_params = {
                    executor.submit(
                        self.runThroughput, params)
                    : params for params in self.forEachTest()
                }

                for future in concurrent.futures.as_completed(
                        future_to_params):
                    params = future_to_params[future]
                    finishedTests += 1
                    live.update(self.generateTable(finishedTests))

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
                if params.latency != -1:
                    params.status = '[green]SUCCESS'
                else:
                    params.status = '[red]FAILED'
                self.updateDataframe(params)
            self.df.to_csv(f'{self.csvFile}', index=False)

        print(self.df)
        for meshRows in self.meshRowsList:
            for rpmPacketType in self.rpmPacketTypes:
                data_latency = {}
                data_avgLinkUtil = {}
                data_effInjRate = {}
                data_totPkt = {}
                data_injRate = {}
                for traffic in self.trafficPatterns:
                    data_latency[traffic] = {}
                    data_avgLinkUtil[traffic] = {}
                    data_effInjRate[traffic] = {}
                    data_totPkt[traffic] = {}
                    data_injRate[traffic] = {}

                    values_latency = {}
                    values_avgLinkUtil = {}
                    values_effInjRate = {}
                    values_totPkt = {}
                    values_injRate = {}
                    for scheme in self.schemes:
                        data_latency[traffic][scheme] = None
                        data_avgLinkUtil[traffic][scheme] = None
                        data_effInjRate[traffic][scheme] = None
                        data_totPkt[traffic][scheme] = None
                        data_injRate[traffic][scheme] = None
                        values_latency[scheme] = []
                        values_avgLinkUtil[scheme] = []
                        values_effInjRate[scheme] = []
                        values_totPkt[scheme] = []
                        values_injRate[scheme] = []

                    for injRate in self.injRates:
                        for scheme in self.schemes:
                            tmpDF = self.df.loc[
                                (self.df["MESH ROWS"] == meshRows) &
                                (self.df["INJECTION RATE"] == injRate) &
                                (self.df["TRAFFIC PATTERN"] == traffic) &
                                (self.df["RPM PACKET TYPE"] == rpmPacketType) &
                                (self.df["SCHEME"] == scheme)]
                            assert(len(tmpDF) == 1)
                            values_latency[scheme].append(tmpDF['LATENCY'].values[0])
                            values_avgLinkUtil[scheme].append(tmpDF['AVERAGE LINK UTIL'].values[0])
                            values_effInjRate[scheme].append(tmpDF['EFFECTIVE INJECTION RATE'].values[0])
                            values_totPkt[scheme].append(tmpDF['TOTAL INJECTED PACKET'].values[0])
                            values_injRate[scheme].append(injRate)

                        #finished=False
                        #for scheme in self.schemes:
                        #    if values_latency[scheme][-1] > 200:
                        #        finished=True
                        #        break
                        #if finished:
                        #    break

                    for scheme in self.schemes:
                        data_latency[traffic][scheme] = values_latency[scheme]
                        data_avgLinkUtil[traffic][scheme] = values_avgLinkUtil[scheme]
                        data_effInjRate[traffic][scheme] = values_effInjRate[scheme]
                        data_totPkt[traffic][scheme] = values_totPkt[scheme]
                        data_injRate[traffic] = values_injRate[scheme]

                nrows = 2
                ncols = 4
                fig_latency,    axes_latency    = plt.subplots(nrows=nrows, ncols=ncols)
                fig_avgLinkUtil,axes_avgLinkUtil= plt.subplots(nrows=nrows, ncols=ncols)
                fig_effInjRate, axes_effInjRate = plt.subplots(nrows=nrows, ncols=ncols)
                fig_totPkt,     axes_totPkt     = plt.subplots(nrows=nrows, ncols=ncols)

                axes_latency        = axes_latency.ravel()
                axes_avgLinkUtil    = axes_avgLinkUtil.ravel()
                axes_effInjRate     = axes_effInjRate.ravel()
                axes_totPkt         = axes_totPkt.ravel()

                #xticks = [round(x, 3) for x in np.arange(0.005, maxInjRate, 0.005)]
                for i, traffic in enumerate(self.trafficPatterns):
                    self.plot_figure(
                        pd.DataFrame(data_latency[traffic], index=data_injRate[traffic]),
                        data_injRate[traffic],
                        axes_latency[i],
                        f'{traffic}',
                        'Injection Rate',
                        'Average Packet Latency (Cycles)',
                    )

                    self.plot_figure(
                        pd.DataFrame(data_avgLinkUtil[traffic], index=data_injRate[traffic]),
                        data_injRate[traffic],
                        axes_avgLinkUtil[i],
                        f'{traffic}',
                        'Injection Rate',
                        'Average Link Utilization (Operations/Cycles)',
                    )

                    self.plot_figure(
                        pd.DataFrame(data_effInjRate[traffic], index=data_injRate[traffic]),
                        data_injRate[traffic],
                        axes_effInjRate[i],
                        f'{traffic}',
                        'Injection Rate',
                        'Effective Injection Rate',
                    )

                    self.plot_figure(
                        pd.DataFrame(data_totPkt[traffic], index=data_injRate[traffic]),
                        data_injRate[traffic],
                        axes_totPkt[i],
                        f'{traffic}',
                        'Injection Rate',
                        'Total Injected Packets',
                    )

                fig_latency.tight_layout()
                fig_latency.savefig(f'{self.outdirRoot}/latency-{meshRows}x{meshRows}-{rpmPacketType}.png')

                fig_avgLinkUtil.tight_layout()
                fig_avgLinkUtil.savefig(f'{self.outdirRoot}/avgLinkUtil-{meshRows}x{meshRows}-{rpmPacketType}.png')

                fig_effInjRate.tight_layout()
                fig_effInjRate.savefig(f'{self.outdirRoot}/effInjRate-{meshRows}x{meshRows}-{rpmPacketType}.png')

                fig_totPkt.tight_layout()
                fig_totPkt.savefig(f'{self.outdirRoot}/totPkt-{meshRows}x{meshRows}-{rpmPacketType}.png')

    def plot_figure(self, df, xticks, axes, title, xlabel, ylabel):
        ax = df.plot(xticks=xticks, ax=axes, title=title, figsize=(12, 9))
        ax.set_xticklabels(xticks, rotation=45)
        ax.set_xlabel(xlabel) #fontdict={'fontsize':9}
        ax.set_ylabel(ylabel) #fontdict={'fontsize':9}

    def getStats(self, params):
        simCycles = 0
        totalInjectedPacket = 0
        totalNodes = params.meshRows * params.meshRows
        with open(params.statFile, "r") as f:
            for line in f:
                if 'simTicks' in line:
                    line = " ".join(line.strip().split())
                    simCycles = float(line.split(' ')[1]) / 500.0

                if 'packets_injected::total' in line:
                    line = " ".join(line.strip().split())
                    params.totalInjectedPacket = float(line.split(' ')[1])

                if 'average_packet_latency' in line:
                    line = " ".join(line.strip().split())
                    params.latency = float(line.split(' ')[1]) / 500.0

                if 'avg_link_utilization' in line:
                    line = " ".join(line.strip().split())
                    params.avgLinkUtil = float(line.split(' ')[1])
       
        if params.latency > 0 and params.totalInjectedPacket != 0 \
            and totalNodes != 0 and simCycles != 0:
            params.effInjRate = params.totalInjectedPacket/totalNodes/simCycles
        return params

    def runThroughput(self, params):

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
        if params.latency > 0:
            params.status = '[green]SUCCESS'
        else:
            params.status = '[red]FAILED'
        self.updateDataframe(params)

    def forEachTest(self):
        for meshRows in self.meshRowsList:
            for scheme in self.schemes:
                for trafficPattern in self.trafficPatterns:
                    for rpmPacketType in self.rpmPacketTypes:
                        for injRate in self.injRates:
                            outdir = f'{self.outdirRoot}/{meshRows}x{meshRows}/{scheme}/'
                            outdir+=f'{rpmPacketType}/{trafficPattern}/{injRate}'
                            p =  Params(
                                meshRows=meshRows,
                                scheme=scheme,
                                trafficPattern=trafficPattern,
                                rpmPacketType=rpmPacketType,
                                injRate=injRate,
                                outdir=outdir,)
                            self.makeGem5CMD(p)
                            yield p

    def makeGem5CMD(self, p):

        cmd = f'{self.cwd}/build/NULL/gem5.{self.opt} \\\n'
        cmd += f'--outdir={p.outdir} \\\n'
        cmd += f'{self.cwd}/configs/example/garnet_synth_traffic.py \\\n'
        cmd += f'--num-cpus={p.meshRows*p.meshRows} \\\n'
        cmd += f'--num-dirs={p.meshRows*p.meshRows} \\\n'
        cmd += f'--network=garnet \\\n'
        cmd += f'--topology=Mesh_XY \\\n'
        cmd += f'--mesh-rows={p.meshRows} \\\n'
        cmd += f'--router-latency=1 \\\n'
        cmd += f'--link-latency=1 \\\n'
        cmd += f'--vcs-per-vnet=4 \\\n'
        cmd += f'--link-width-bits=128 \\\n'
        cmd += f'--garnet-deadlock-threshold=10000000 \\\n'
        cmd += f'--synthetic={p.trafficPattern} \\\n'
        cmd += f'--sim-cycles=1000000 \\\n'
        #cmd += f'--num-packets-max=50000000 \\\n'
        cmd += f'--injectionrate={p.injRate} \\\n'
        cmd += f'--num-packets-max=-1 \\\n'
        cmd += f'--single-sender-id=-1 \\\n'
        cmd += f'--single-dest-id=-1 \\\n'
        cmd += f'--inj-vnet=2 \\\n'
        cmd += f'--rpm-packet-type={p.rpmPacketType} \\\n'

        if p.scheme == 'base':
            cmd += f'--routing-algorithm=1 \\\n'
        else:
            cmd += f'--routing-algorithm=2 \\\n'
            cmd += f'--enable-rpm \\\n'
        cmd += f'> {p.outdir}/log 2>&1\n'

        p.gem5CMD= cmd
        p.statFile = f'{p.outdir}/stats.txt'

    def generateTable(self, finishedTests) -> Table:
        self.lock.acquire()
        """Make a new Table"""
        table = Table()
        table.add_column("MESH ROWS")
        table.add_column("SCHEME")
        table.add_column("TRAFFIC PATTERN")
        table.add_column("RPM PACKET TYPE")
        table.add_column("INJECTION RATE")
        table.add_column("STATUS")

        progress = (finishedTests/float(self.totalTests))
        table.title = f'[not italic]:popcorn:[/]Garnet throughput simulation '
        table.title += f'{progress:.0%} ({finishedTests}/{self.totalTests})'
        table.title += f'[not italic]:popcorn:[/]'

        for index, row in self.df.iterrows():
            if 'SUCCESS' not in row['STATUS']:
                latency = row['LATENCY']
                table.add_row(
                    f"{row['MESH ROWS']}",
                    f"{row['SCHEME']}",
                    f"{row['TRAFFIC PATTERN']}",
                    f"{row['RPM PACKET TYPE']}",
                    f"{row['INJECTION RATE']:14.3f}",
                    f"{row['STATUS']}",
            )
        self.lock.release()
        return table

    def updateDataframe(self, params):

        self.lock.acquire()

        if len(self.df.loc[
            (self.df['MESH ROWS'] == params.meshRows)
                & (self.df['SCHEME'] == params.scheme)
                & (self.df['TRAFFIC PATTERN'] == params.trafficPattern)
                & (self.df['RPM PACKET TYPE'] == params.rpmPacketType)
                & (self.df['INJECTION RATE'] == params.injRate)]) == 0:
            self.df = self.df.append(
                self.getNewDataframeRow(params),
                ignore_index=True
            )
        else:
            self.df.loc[(self.df['MESH ROWS'] == params.meshRows)
                & (self.df['SCHEME'] == params.scheme)
                & (self.df['TRAFFIC PATTERN'] == params.trafficPattern)
                & (self.df['RPM PACKET TYPE'] == params.rpmPacketType)
                & (self.df['INJECTION RATE'] == params.injRate)] \
            = [
                params.meshRows,
                params.scheme,
                params.trafficPattern,
                params.rpmPacketType,
                params.injRate,
                params.effInjRate,
                params.totalInjectedPacket,
                params.status,
                params.latency,
                params.avgLinkUtil
            ]

        self.lock.release()

    def getNewDataframeRow(self, params):
        return {
            'MESH ROWS':params.meshRows,
            'SCHEME':params.scheme,
            'TRAFFIC PATTERN':params.trafficPattern,
            'RPM PACKET TYPE':params.rpmPacketType,
            'INJECTION RATE':params.injRate,
            'EFFECTIVE INJECTION RATE':params.effInjRate,
            'TOTAL INJECTED PACKET':params.totalInjectedPacket,
            'STATUS':params.status,
            'LATENCY':params.latency,
            'AVERAGE LINK UTIL':params.avgLinkUtil,
        }
    def getNewDataframe(self):
        return pd.DataFrame(columns=[
            'MESH ROWS',
            'SCHEME',
            'TRAFFIC PATTERN',
            'RPM PACKET TYPE',
            'INJECTION RATE',
            'EFFECTIVE INJECTION RATE',
            'TOTAL INJECTED PACKET',
            'STATUS',
            'LATENCY',
            'AVERAGE LINK UTIL',
        ])

#################################################################
# Sub commands
#################################################################
def build(args):
    cmd = f'scons {os.getcwd()}/build/NULL/gem5.{args.opt} '
    cmd += 'PROTOCOL=Garnet_standalone -j16'
    os.system(cmd)

def simThroughput(args):
    rpm = RPMCLI(args)
    rpm.simThroughput()
    rpm.plot()

def plot(args):
    rpm = RPMCLI(args)
    rpm.plot()

def main(argv: List[str] = None):
    parser = argparse.ArgumentParser(
        prog=progName,
        description="RPM Commandline Interface",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )
    parser.set_defaults(func=lambda x: parser.print_help())
    #################################################################
    # Common arguments
    #################################################################
    parser.add_argument('--opt', action='store',
                            type=str, default='debug',
                            help="Build options (debug/opt/fast).")
    parser.add_argument('--schemes', action='store',
                            type=str, nargs='+',
                            choices=['base', 'rpm'],
                            default=['base', 'rpm'],
                            help="List of schems.")
    parser.add_argument('--traffic-patterns', action='store',
                            type=str, nargs='+',
                            choices=[
                               'uniform_random',
                               'tornado',
                               'bit_complement',
                               'bit_reverse',
                               'bit_rotation',
                               'neighbor',
                               'shuffle',
                               'transpose'],
                            default=[
                               'uniform_random',
                               'tornado',
                               'bit_complement',
                               'bit_reverse',
                               'bit_rotation',
                               'neighbor',
                               'shuffle',
                               'transpose'],
                            help="List of schems.")
    parser.add_argument('--mesh-rows', action='store',
                            type=int, nargs='+', default=[4,8],
                            help="List of mesh rows to simulate.")
    parser.add_argument('--rpm-packet-types', action='store',
                            type=int, nargs='+', default=[0,1,2],
                            help="""types of rpm packetself.
                            1: multicast packet.
                            2: broadcast packet. Every packet is broadcast.""")
    parser.add_argument('--outdir-root',
                            type=str, default='./m5out', help="",)
    parser.add_argument('--max-workers',
                            type=int, default=16, help="",)

    subparsers = parser.add_subparsers(title='commands')
    #################################################################
    # Sub command - build
    #################################################################
    parser_build = subparsers.add_parser('build', help='Make gem5 executable.')
    parser_build.set_defaults(func=build)
    #################################################################
    # Sub command - sim-throughput
    #################################################################
    parser_simThroughput = subparsers.add_parser('sim-throughput',
                                help='Start garnet throughput simulation')
    parser_simThroughput.set_defaults(func=simThroughput)

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
