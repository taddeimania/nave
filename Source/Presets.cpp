#include "Presets.h"
#include "Params.h"
#include "Settings.h"

namespace nave
{
namespace
{
using Values = std::vector<std::pair<const char*, float>>;

// A preset starts from its type's starting point; `extra` overrides.
FactoryPreset make (const char* name, const char* blurb, int type, Values extra)
{
    using namespace ids;
    const auto& st = typeInfo (type).start;
    Values v { { ids::type, (float) type }, { decay, st.decay }, { size, st.size }, { diffusion, st.diffusion },
               { early, st.early }, { charA, st.a }, { charB, st.b }, { charC, st.c }, { charMode, (float) st.mode } };
    for (const auto& [id, value] : extra)
    {
        bool replaced = false;
        for (auto& [vid, vv] : v)
            if (juce::String (vid) == id) { vv = value; replaced = true; }
        if (! replaced) v.push_back ({ id, value });
    }
    return { name, blurb, v };
}
} // namespace

const std::vector<FactoryPreset>& factoryPresets()
{
    using namespace ids;
    static const std::vector<FactoryPreset> list {
        make ("Start Here", "A warm concert hall, mixed in behind the sound.", TConcertHall, {}),

        // Rooms
        make ("Natural Ambience", "Just enough room to make a dry track sound recorded.", TAmbience, { { mix, 0.32f } }),
        make ("Studio Live", "A flattering tracking room for drums, guitars and voices.", TStudio, { { mix, 0.3f }, { tone, 0.55f } }),
        make ("Drum Room Smash", "A bright room crushed hard behind the kit.", TDrumRoom,
              { { charC, 0.75f }, { mix, 0.35f }, { lowCut, 90.0f }, { tone, 0.6f } }),
        make ("Echo Chamber", "Smooth, dense concrete chamber for vocals and strings.", TChamber,
              { { decay, 2.2f }, { predelay, 25.0f }, { mix, 0.28f }, { lowCut, 150.0f } }),
        make ("Club Night", "A lively venue with the bass hanging around.", TClub, { { mix, 0.3f } }),
        make ("Move It Back", "The source steps back and left in a small room. Try moving X and Y.", TReflections,
              { { charA, 0.3f }, { charB, 0.8f }, { early, 0.75f }, { mix, 0.3f } }),

        // Halls
        make ("Recital Hall", "A small hall, quick to bloom: piano and acoustic instruments.", TSmallHall,
              { { predelay, 15.0f }, { mix, 0.28f } }),
        make ("Arena Rock", "Huge and booming, held back with pre-delay so the vocal stays up front.", TArena,
              { { predelay, 70.0f }, { mix, 0.22f }, { lowCut, 120.0f } }),
        make ("Cathedral Echo", "Stone, height and a slap from the far wall.", TCathedral,
              { { decay, 9.0f }, { charC, 0.55f }, { mix, 0.35f } }),
        make ("Ducked Vocal Hall", "A long hall that ducks out of the way while the vocal sings.", TConcertHall,
              { { decay, 3.8f }, { predelay, 45.0f }, { duck, 0.6f }, { release, 450.0f }, { mix, 0.35f }, { lowCut, 180.0f } }),

        // Plates
        make ("Vocal Plate", "The classic bright vocal plate, a little pre-delay, low end trimmed.", TVocalPlate,
              { { predelay, 40.0f }, { lowCut, 200.0f }, { mix, 0.25f } }),
        make ("Snare Plate", "Short, splashy and bright on drums.", TPlate,
              { { decay, 1.3f }, { tone, 0.62f }, { predelay, 8.0f }, { mix, 0.3f }, { charMode, 1.0f } }),
        make ("Vintage Plate Drive", "A dark old plate pushed through its driver.", TVintagePlate,
              { { charA, 0.65f }, { mix, 0.3f } }),

        // Springs
        make ("Combo Spring", "Two springs on a combo amp: warm and drippy.", TSpring,
              { { decay, 3.0f }, { tone, 0.4f }, { charA, 0.35f }, { charMode, 1.0f }, { mix, 0.35f } }),
        make ("Surf's Up", "Three springs driven hard and mixed loud.", TSurfSpring, { { mix, 0.5f }, { tone, 0.7f }, { lowEnd, -4.0f } }),

        // Ambient
        make ("Endless Cloud", "A slow, soft cloud. Hold a chord and let it hang.", TCloud,
              { { decay, 20.0f }, { mix, 0.5f }, { modDepth, 0.45f } }),
        make ("Ambient Bloom", "Notes open up like flowers after you play them.", TBloom,
              { { charA, 0.7f }, { charB, 0.5f }, { mix, 0.45f } }),
        make ("Shimmer Pad", "An octave up in the tail, climbing slowly.", TShimmer, { { mix, 0.45f }, { modDepth, 0.4f } }),
        make ("Halo Strings", "Octave and twelfth shimmering above a soft cloud.", THalo, { { mix, 0.5f } }),
        make ("Into the Abyss", "An octave down and very dark. Cinematic.", TAbyss, { { mix, 0.45f } }),
        make ("Choir Loft", "A choir sings back, drifting between AH and OH.", TChoir,
              { { charMode, 4.0f }, { charA, 0.55f }, { charB, 0.7f }, { mix, 0.38f }, { modDepth, 0.5f } }),
        make ("Swell Behind", "The reverb fades in behind each note.", TSwell, { { mix, 0.5f }, { charA, 0.55f } }),
        make ("Violin Swells", "The dry note itself fades in, into a long wash. No volume pedal needed.", TSwell,
              { { charMode, 1.0f }, { charA, 0.62f }, { decay, 10.0f }, { mix, 0.55f }, { modDepth, 0.55f } }),

        // Special
        make ("Gated Snare", "The eighties gated drum sound.", TGated, { { mix, 0.4f }, { tone, 0.6f } }),
        make ("Reverse Lead", "Rises into each note and stops.", TReverse, { { mix, 0.45f } }),
        make ("Bouncing Shape", "An upside-down bell, repeating.", TNonlinear,
              { { charMode, 5.0f }, { charB, 0.55f }, { charC, 0.3f }, { mix, 0.4f } }),
        make ("Space Echo Room", "Four heads of tape echo smeared into a small room.", TTape,
              { { charMode, 2.0f }, { charB, 0.45f }, { mix, 0.4f } }),
        make ("Slapback", "One quick tape slap and a little room. Rockabilly vocals.", TTape,
              { { charA, 0.27f }, { charB, 0.08f }, { charMode, 0.0f }, { diffusion, 0.05f }, { early, 0.2f }, { mix, 0.35f } }),
        make ("Broken Cassette", "Crushed, warped and gritty.", TLofi, { { charA, 0.55f }, { charB, 0.6f }, { mix, 0.45f } }),
        make ("Tuned in A Minor", "The tail rings on an A minor chord.", TResonant,
              { { charMode, 9.0f }, { charB, 0.55f }, { mix, 0.45f } }),

        // For send/return tracks.
        make ("Send: Hall", "100% wet for a return track. Lock the mix with KEEP MIX.", TConcertHall,
              { { mix, 1.0f }, { lowCut, 150.0f }, { predelay, 25.0f } }),
        make ("Send: Plate", "100% wet plate for a return track.", TPlate, { { mix, 1.0f }, { lowCut, 180.0f } }),
    };
    return list;
}

juce::File userPresetFolder()
{
    return Settings::file().getParentDirectory().getChildFile ("Presets");
}

juce::Array<juce::File> userPresets()
{
    auto files = userPresetFolder().findChildFiles (juce::File::findFiles, false, juce::String ("*") + kPresetExtension);
    files.sort();
    return files;
}
} // namespace nave
