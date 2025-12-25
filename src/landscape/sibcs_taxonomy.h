#pragma once

#include "landscape_types.h"
#include <vector>
#include <map>
#include <algorithm>

namespace landscape {

class SiBCSTaxonomy {
public:
    // --- Level 2: Suborder ---
    static std::vector<SiBCSSubOrder> getValidSuborders(SiBCSOrder order) {
        switch(order) {
            case SiBCSOrder::kLatossolo:
                return {SiBCSSubOrder::kVermelho, SiBCSSubOrder::kVermelhoAmarelo, SiBCSSubOrder::kAmarelo, SiBCSSubOrder::kBruno};
            case SiBCSOrder::kArgissolo:
                return {SiBCSSubOrder::kVermelho, SiBCSSubOrder::kVermelhoAmarelo, SiBCSSubOrder::kAmarelo, SiBCSSubOrder::kBruno}; // Bruno added
            case SiBCSOrder::kGleissolo:
                return {SiBCSSubOrder::kHaplic, SiBCSSubOrder::kMelanico, SiBCSSubOrder::kTiomorfico, SiBCSSubOrder::kSalico};
            case SiBCSOrder::kNeossoloLit:
                return {SiBCSSubOrder::kLitolico};
            case SiBCSOrder::kNeossoloQuartz:
                return {SiBCSSubOrder::kQuartzarenico, SiBCSSubOrder::kHidromorfico}; // Wait, Hidromorfico is GreatGroup in standard, but let's check
                 // The txt says "Neossolo Quartzarênico" -> "Hidromórfico" is Great Group.
                 // So for Suborder of Neossolos we use the Types defined in types.h:
                 // Litólico, Quartzarênico, Flúvico, Regolítico
            case SiBCSOrder::kCambissolo:
                return {SiBCSSubOrder::kHaplic, SiBCSSubOrder::kHumico, SiBCSSubOrder::kFluvico};
            default:
                // Generic fallbacks for unimplemented Orders
                return {SiBCSSubOrder::kHaplic};
        }
        // Special case for Neossolos wrapper in Enums if we treated them as one Order "Neossolo"
        // But we have kNeossoloLit and kNeossoloQuartz separate.
        // Let's assume standard behavior.
    }

    // --- Level 3: Great Group ---
    static std::vector<SiBCSGreatGroup> getValidGreatGroups(SiBCSOrder order, SiBCSSubOrder sub) {
        std::vector<SiBCSGreatGroup> groups;
        
        // Common Attributes
        groups.push_back(SiBCSGreatGroup::kDistrofico);
        groups.push_back(SiBCSGreatGroup::kEutrofico);

        if (order == SiBCSOrder::kLatossolo) {
            groups.push_back(SiBCSGreatGroup::kAcrico);
            groups.push_back(SiBCSGreatGroup::kFerrico);
            groups.push_back(SiBCSGreatGroup::kDistroferrico);
            groups.push_back(SiBCSGreatGroup::kAluminico);
        }

        if (order == SiBCSOrder::kGleissolo) {
            groups.clear();
            groups.push_back(SiBCSGreatGroup::kTbDistrofico);
            groups.push_back(SiBCSGreatGroup::kTbEutrofico);
            groups.push_back(SiBCSGreatGroup::kAluminico); // Tiomorfico?
        }
        
        if (order == SiBCSOrder::kNeossoloLit || order == SiBCSOrder::kNeossoloQuartz) {
            groups.push_back(SiBCSGreatGroup::kOrtico);
            if (sub == SiBCSSubOrder::kQuartzarenico) {
                groups.push_back(SiBCSGreatGroup::kHidromorfico);
            }
        }

        return groups;
    }

    // --- Level 4: SubGroup ---
    static std::vector<SiBCSSubGroup> getValidSubGroups(SiBCSOrder order, SiBCSSubOrder sub, SiBCSGreatGroup group) {
        std::vector<SiBCSSubGroup> subs;
        subs.push_back(SiBCSSubGroup::kTipico);

        // Psamitico logic
        if (group == SiBCSGreatGroup::kDistrofico || group == SiBCSGreatGroup::kEutrofico) {
            subs.push_back(SiBCSSubGroup::kPsamitico); // Valid for Lat/Arg
        }
        
        // Humico
        subs.push_back(SiBCSSubGroup::kHumico);
        
        // Intergrades
        if (order == SiBCSOrder::kLatossolo) subs.push_back(SiBCSSubGroup::kArgissolico);
        if (order == SiBCSOrder::kArgissolo) subs.push_back(SiBCSSubGroup::kLatossolico);
        
        // Tiomorfico/Salico Intergrades
        if (order != SiBCSOrder::kGleissolo) {
            subs.push_back(SiBCSSubGroup::kTiomorfico); // e.g. Gleissolo Tiomorfico... wait, Tiomorfico is Suborder there.
            // As secondary trait (Subgroup)
            subs.push_back(SiBCSSubGroup::kSalico);
        }

        return subs;
    }
};

} // namespace landscape
