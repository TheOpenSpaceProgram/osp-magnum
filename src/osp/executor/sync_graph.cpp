/**
 * Open Space Program
 * Copyright © 2019-2025 Open Space Program Project
 *
 * MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "sync_graph.h"

#include <cmath>
#include <iostream>

namespace osp::exec
{
void SyncGraph::debug_verify() const
{
    bool nothingWentWrong = true;
    for (SynchronizerId const syncId : syncIds)
    {
        for (SubgraphPointAddr const& addr : syncs.at(syncId).connectedPoints)
        {
            auto const& connectedSyncs = subgraphs.at(addr.subgraph).points.at(addr.point).connectedSyncs;
            if (std::find(connectedSyncs.begin(), connectedSyncs.end(), syncId) == connectedSyncs.end())
            {
                nothingWentWrong = false;
                std::cerr << "Graph::debug_verify: Missing subgraph->sync connection: "
                          << "(" << addr.subgraph.value << "->" << syncId.value << "): "
                          << subgraphs.at(addr.subgraph).debugName << " -> " << syncs.at(syncId).debugName << "\n";
            }
        }
    }

    for (SubgraphId const subgraphId : subgraphIds)
    {
        Subgraph const &subgraph = subgraphs.at(subgraphId);

        if ( ! subgraph.instanceOf.has_value() )
        {
            nothingWentWrong = false;
            std::cerr << "Graph::debug_verify: subgraph has no instance type: (" << subgraphId.value << "): " << subgraph.debugName << "\n";
            continue;
        }

        if (subgraph.points.size() != sgtypes.at(subgraph.instanceOf).points.size())
        {
            nothingWentWrong = false;
            std::cerr << "Graph::debug_verify: wrong number of points" << subgraphId.value << "): " << subgraph.debugName << "\n";
            continue;
        }

        for (std::size_t i = 0; i < subgraph.points.size(); ++i)
        {
            LocalPointId const pointId = LocalPointId::from_index(i);
            Subgraph::Point const &point = subgraph.points[pointId];

            for (SynchronizerId const syncId : point.connectedSyncs)
            {
                bool found = false;
                for (SubgraphPointAddr const& addr : syncs.at(syncId).connectedPoints)
                {
                    if (addr.subgraph == subgraphId && addr.point == pointId)
                    {
                        found = true;
                        break;
                    }
                }

                if ( ! found )
                {
                    nothingWentWrong = false;
                    std::cerr << "Graph::debug_verify: Missing sync->subgraph connection: "
                          << "(" << syncId.value << "->" << subgraphId.value << "): "
                          << syncs.at(syncId).debugName << " -> " << subgraph.debugName << "\n";
                }
            }
        }
    }

    LGRN_ASSERT(nothingWentWrong);
}

constexpr std::initializer_list<std::string_view> sc_colorPalette = {
    "#DAA520", "#8FBC8F", "#800080", "#B03060", "#D2B48C", "#66CDAA", "#9932CC", "#FF0000",
    "#FF8C00", "#FFD700", "#FFFF00", "#C71585", "#0000CD", "#7FFF00", "#00FF00", "#BA55D3",
    "#00FA9A", "#4169E1", "#DC143C", "#00FFFF", "#00BFFF", "#9370DB", "#0000FF", "#A020F0",
    "#FF6347", "#D8BFD8", "#FF00FF", "#1E90FF", "#DB7093", "#EEE8AA", "#FFFF54", "#DDA0DD",
    "#696969", "#A9A9A9", "#2F4F4F", "#556B2F", "#6B8E23", "#A0522D", "#A52A2A", "#2E8B57",
    "#191970", "#808000", "#483D8B", "#008000", "#BC8F8F", "#663399", "#008080", "#BDB76B",
    "#CD853F", "#4682B4", "#D2691E", "#9ACD32", "#20B2AA", "#CD5C5C", "#00008B", "#32CD32",
    "#B0E0E6", "#FF1493", "#FFA07A", "#EE82EE", "#98FB98", "#87CEFA", "#7FFFD4", "#FF69B4"
};

std::ostream& operator<<(std::ostream& rStream, SyncGraphDOTVisualizer const& self)
{
    rStream << "graph G {\n"
            << "    rankdir=LR ranksep=0 nodesep=0.25\n"
               "    edge[fontcolor=white,fontname=\"Helvetica,Arial,sans-serif\"]\n"
               "    node[fontcolor=white,fontname=\"Helvetica,Arial,sans-serif\"]\n"
               "    bgcolor=\"#181122\"\n"
               "    node[shape=\"rectangle\",style=filled,fillcolor=grey28,penwidth=0]\n";

    rStream << "    edge[minlen=2 color=\"#47474780\" penwidth=15 weight=300]\n";

    for (SubgraphId const subgraphId : self.graph.subgraphIds)
    {
        Subgraph        const &subgraph = self.graph.subgraphs[subgraphId];
        SubgraphType    const &sgtype   = self.graph.sgtypes[subgraph.instanceOf];
        LocalPointId    const currentPoint = (self.pDebugInfo == nullptr) ? LocalPointId{} : self.pDebugInfo->current_point(self.graph, subgraphId);

        for (int i = 0; i < sgtype.points.size(); ++i)
        {
            auto const pointId = LocalPointId::from_index(i);
            rStream << "    sg" << subgraphId.value << "p" << i << "[label=\"" << sgtype.points[pointId].debugName << "\"";

            if (pointId == currentPoint)
            {
                rStream << " color=red penwidth=4";
            }

            if (i == 0)
            {
                rStream << " xlabel=\"[SGT" << subgraph.instanceOf.value << "] " << sgtype.debugName << "\n" << "[SG" << subgraphId.value << "] " << subgraph.debugName << "\"";
            }
            rStream << "]\n";
        }

        rStream << "    ";
        for (int i = 0; i < sgtype.points.size(); ++i)
        {
            if (i != 0)
            {
                rStream << " -- ";
            }
            rStream << "sg" << subgraphId.value << "p" << i;
        }
        rStream << "\n";
    }

    int color = 0;

    rStream << "    edge[minlen=1 penwidth=5 weight=20] node[style=filled]\n";

    for (SynchronizerId const syncId : self.graph.syncIds)
    {
        Synchronizer const &sync = self.graph.syncs[syncId];
        bool const enabled = (self.pDebugInfo == nullptr) ? true : self.pDebugInfo->is_sync_enabled(self.graph, syncId);
        bool const locked = (self.pDebugInfo == nullptr) ? false : self.pDebugInfo->is_sync_locked(self.graph, syncId);

        auto       it    = sync.connectedPoints.begin();
        auto const &last = sync.connectedPoints.end();

        if (sync.connectedPoints.size() == 1)
        {

            rStream << "    sylabel" << syncId.value << "[height=0.1 width=0.1 fillcolor=\"" << sc_colorPalette.begin()[color] << (enabled ? "80" : "10") << "\" "
                    << "label=\"" << syncId.value << ": " << sync.debugName << "\"" "];\n"
                    << "    sylabel" << syncId.value
                    << " -- "
                    << "sg" << it->subgraph.value << "p" << int(it->point.value)
                    << "[weight=50 minlen=0 "
                    << "color=\""<< sc_colorPalette.begin()[color] << (enabled ? "FF" : "10") << "\" "
                    << (locked ? "style=dashed" : "")
                    << "];\n";
        }
        else if (it != last)
        {
            bool first = true;
            while (true)
            {
                auto const next = it + 1;
                if (next != last)
                {
                    rStream << "    sg" << it->subgraph.value << "p" << int(it->point.value)
                            << " -- "
                            << "sg" << next->subgraph.value << "p" << int(next->point.value)
                            << "[color=\"" << sc_colorPalette.begin()[color] << (enabled ? "FF" : "10") << "\" "
                            << (locked                     ? "style=dashed " : "")
                            << (sync.debugGraphLongAndUgly ? "constraint=false weight=0 " : "")
                            << (sync.debugGraphLoose       ? "minlen=0 " : "");

                    if (first)
                    {
                        rStream << "xlabel=<<TABLE BORDER=\"0\" BGCOLOR=\"" << sc_colorPalette.begin()[color] << (enabled ? "80" : "10") << "\"><TR><TD>" << syncId.value << ": " << sync.debugName << "</TD></TR></TABLE>>";
                    }
                    else
                    {
                        rStream << "xlabel=" << syncId.value;
                    }
                    rStream << "]\n";
                    first = false;
                    it = next;
                }
                else
                {
                    break;
                }
            }

            if (sync.debugGraphStraight)
            {

                rStream << "    {\n"
                        << "        rank=same\n"
                        << "        ";
                bool first = true;
                for (SubgraphPointAddr const addr : sync.connectedPoints)
                {
                    if (!first)
                    {
                        rStream << " ";
                    }
                    rStream << "sg" << addr.subgraph.value << "p" << int(addr.point.value);
                    first = false;
                }
                rStream << "\n"
                        << "    }\n";
            }
        }

        color = (color + 1) % sc_colorPalette.size();
    }


    rStream << "}\n";

    return rStream;
}

} // namespace osp::sync

