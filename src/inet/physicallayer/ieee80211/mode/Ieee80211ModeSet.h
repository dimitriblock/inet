//
// Copyright (C) 2014 OpenSim Ltd.
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

#ifndef __INET_IEEE80211MODESET_H
#define __INET_IEEE80211MODESET_H

#include "inet/physicallayer/ieee80211/mode/IIeee80211Mode.h"

namespace inet {

namespace physicallayer {

class INET_API Ieee80211ModeSet
{
  protected:
    class INET_API Entry
    {
      public:
        bool isMandatory;
        const IIeee80211Mode *mode;
    };

  protected:
    const char name;
    const std::vector<Entry> entries;

  public:
    static const std::vector<Ieee80211ModeSet> modeSets;

  protected:
    int getModeIndex(const IIeee80211Mode *mode) const;

  public:
    Ieee80211ModeSet(char name, const std::vector<Entry> entries);

    const char getName() const { return name; }

    const IIeee80211Mode *getMode(bps bitrate) const;
    const IIeee80211Mode *getSlowestMode() const;
    const IIeee80211Mode *getFastestMode() const;
    const IIeee80211Mode *getSlowerMode(const IIeee80211Mode *mode) const;
    const IIeee80211Mode *getFasterMode(const IIeee80211Mode *mode) const;

    static const Ieee80211ModeSet *getModeSet(char mode);
};

} // namespace physicallayer

} // namespace inet

#endif // ifndef __INET_IEEE80211MODESET_H
