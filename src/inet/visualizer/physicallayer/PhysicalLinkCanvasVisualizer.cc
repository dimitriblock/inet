//
// Copyright (C) OpenSim Ltd.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program; if not, see <http://www.gnu.org/licenses/>.
//

#include "inet/physicallayer/contract/packetlevel/IRadio.h"
#include "inet/visualizer/physicallayer/PhysicalLinkCanvasVisualizer.h"

namespace inet {

namespace visualizer {

Define_Module(PhysicalLinkCanvasVisualizer);

bool PhysicalLinkCanvasVisualizer::isLinkEnd(cModule *module) const
{
    return dynamic_cast<inet::physicallayer::IRadio *>(module) != nullptr;
}

const LinkVisualizerBase::LinkVisualization *PhysicalLinkCanvasVisualizer::createLinkVisualization(cModule *source, cModule *destination) const
{
    auto linkVisualization = static_cast<const LinkCanvasVisualization *>(LinkCanvasVisualizerBase::createLinkVisualization(source, destination));
    linkVisualization->figure->setTags("physical_link");
    linkVisualization->figure->setTooltip("This arrow represents a physical link between two network nodes");
    return linkVisualization;
}

} // namespace visualizer

} // namespace inet

