#pragma once

#include <algorithm>
#include <map>

namespace vesper
{
    enum class IndexOfRefraction
    {
        Vacuum,
        Air,
        Ice,
        Water,
        Glass,
        Sapphire,
        Diamond
    };

    struct ComplexIOR
    {
        RgbSpectrum eta;
        RgbSpectrum k;
    };

    class Fresnel
    {
    public:
        inline static float dielectric(float cosThetaI, float extIOR, float intIOR, float& cosThetaT)
        {
            float etaI = extIOR, etaT = intIOR;

            // If indices of refraction are the same, no fresnel effects
            if (extIOR == intIOR)
                return 0.0f;

            // if cosThetaI is < 0, it means the ray is coming from inside the object
            if (cosThetaI < 0.0f)
            {
                std::swap(etaI, etaT);
                cosThetaI = -cosThetaI;
            }

            float eta = etaI / etaT;
            float sinThetaTSqr = eta * eta * (1.0f - cosThetaI * cosThetaI);

            // Total internal reflection
            if (sinThetaTSqr > 1.0f)
                return 1.0f;

            cosThetaT = std::sqrtf(1.0f - sinThetaTSqr);

            float Rs = (etaI * cosThetaI - etaT * cosThetaT) / (etaI * cosThetaI + etaT * cosThetaT);
            float Rp = (etaT * cosThetaI - etaI * cosThetaT) / (etaT * cosThetaI + etaI * cosThetaT);
            return (Rs * Rs + Rp * Rp) / 2.0f;
        }

        inline static float dielectric(float cosThetaI, float extIOR, float intIOR)
        {
            float etaI = extIOR, etaT = intIOR;

            // If indices of refraction are the same, no fresnel effects
            if (extIOR == intIOR)
                return 0.0f;

            // if cosThetaI is < 0, it means the ray is coming from inside the object
            if (cosThetaI < 0.0f)
            {
                std::swap(etaI, etaT);
                cosThetaI = -cosThetaI;
            }

            float eta = etaI / etaT;
            float sinThetaTSqr = eta * eta * (1.0f - cosThetaI * cosThetaI);

            // Total internal reflection
            if (sinThetaTSqr > 1.0f)
                return 1.0f;

            float cosThetaT = std::sqrtf(1.0f - sinThetaTSqr);

            float Rs = (etaI * cosThetaI - etaT * cosThetaT) / (etaI * cosThetaI + etaT * cosThetaT);
            float Rp = (etaT * cosThetaI - etaI * cosThetaT) / (etaT * cosThetaI + etaI * cosThetaT);
            return (Rs * Rs + Rp * Rp) / 2.0f;
        }

        inline static float getIOR(IndexOfRefraction iorMaterial)
        {
            switch (iorMaterial)
            {
            case IndexOfRefraction::Vacuum:   return 1.0f;
            case IndexOfRefraction::Air:      return 1.00029f;
            case IndexOfRefraction::Ice:      return 1.31f;
            case IndexOfRefraction::Water:    return 1.33f;
            case IndexOfRefraction::Glass:    return 1.5046f;
            case IndexOfRefraction::Sapphire: return 1.77f;
            case IndexOfRefraction::Diamond:  return 2.417f;
            default:                          return 1.0f;
            }
        };

        inline static ComplexIOR getComplexIOR(std::string name)
        {
            static const std::map<std::string, ComplexIOR> complexIorMap =
            {
                { "a-C"   , { { 2.9440999183f, 2.2271502925f, 1.9681668794f }, { 0.8874329109f, 0.7993216383f, 0.8152862927f } } },
                { "Ag"    , { { 0.1552646489f, 0.1167232965f, 0.1383806959f }, { 4.8283433224f, 3.1222459278f, 2.1469504455f } } },
                { "Al"    , { { 1.6574599595f, 0.8803689579f, 0.5212287346f }, { 9.2238691996f, 6.2695232477f, 4.8370012281f } } },
                { "AlAs"  , { { 3.6051023902f, 3.2329365777f, 2.2175611545f }, {0.0006670247f, -0.0004999400f, 0.0074261204f } } },
                { "AlSb"  , { {-0.0485225705f, 4.1427547893f, 4.6697691348f }, {-0.0363741915f, 0.0937665154f, 1.3007390124f } } },
                { "Au"    , { { 0.1431189557f, 0.3749570432f, 1.4424785571f }, { 3.9831604247f, 2.3857207478f, 1.6032152899f } } },
                { "Be"    , { { 4.1850592788f, 3.1850604423f, 2.7840913457f }, { 3.8354398268f, 3.0101260162f, 2.8690088743f } } },
                { "Cr"    , { { 4.3696828663f, 2.9167024892f, 1.6547005413f }, { 5.2064337956f, 4.2313645277f, 3.7549467933f } } },
                { "CsI"   , { { 2.1449030413f, 1.7023164587f, 1.6624194173f }, { 0.0000000000f, 0.0000000000f, 0.0000000000f } } },
                { "Cu"    , { { 0.2004376970f, 0.9240334304f, 1.1022119527f }, { 3.9129485033f, 2.4528477015f, 2.1421879552f } } },
                { "Cu2O"  , { { 3.5492833755f, 2.9520622449f, 2.7369202137f }, { 0.1132179294f, 0.1946659670f, 0.6001681264f } } },
                { "CuO"   , { { 3.2453822204f, 2.4496293965f, 2.1974114493f }, { 0.5202739621f, 0.5707372756f, 0.7172250613f } } },
                { "d-C"   , { { 2.7112524747f, 2.3185812849f, 2.2288565009f }, { 0.0000000000f, 0.0000000000f, 0.0000000000f } } },
                { "Hg"    , { { 2.3989314904f, 1.4400254917f, 0.9095512090f }, { 6.3276269444f, 4.3719414152f, 3.4217899270f } } },
                { "HgTe"  , { { 4.7795267752f, 3.2309984581f, 2.6600252401f }, { 1.6319827058f, 1.5808189339f, 1.7295753852f } } },
                { "Ir"    , { { 3.0864098394f, 2.0821938440f, 1.6178866805f }, { 5.5921510077f, 4.0671757150f, 3.2672611269f } } },
                { "K"     , { { 0.0640493070f, 0.0464100621f, 0.0381842017f }, { 2.1042155920f, 1.3489364357f, 0.9132113889f } } },
                { "Li"    , { { 0.2657871942f, 0.1956102432f, 0.2209198538f }, { 3.5401743407f, 2.3111306542f, 1.6685930000f } } },
                { "MgO"   , { { 2.0895885542f, 1.6507224525f, 1.5948759692f }, {0.0000000000f, -0.0000000000f, 0.0000000000f } } },
                { "Mo"    , { { 4.4837010280f, 3.5254578255f, 2.7760769438f }, { 4.1111307988f, 3.4208716252f, 3.1506031404f } } },
                { "Na"    , { { 0.0602665320f, 0.0561412435f, 0.0619909494f }, { 3.1792906496f, 2.1124800781f, 1.5790940266f } } },
                { "Nb"    , { { 3.4201353595f, 2.7901921379f, 2.3955856658f }, { 3.4413817900f, 2.7376437930f, 2.5799132708f } } },
                { "Ni"    , { { 2.3672753521f, 1.6633583302f, 1.4670554172f }, { 4.4988329911f, 3.0501643957f, 2.3454274399f } } },
                { "Rh"    , { { 2.5857954933f, 1.8601866068f, 1.5544279524f }, { 6.7822927110f, 4.7029501026f, 3.9760892461f } } },
                { "Se-e"  , { { 5.7242724833f, 4.1653992967f, 4.0816099264f }, { 0.8713747439f, 1.1052845009f, 1.5647788766f } } },
                { "Se"    , { { 4.0592611085f, 2.8426947380f, 2.8207582835f }, { 0.7543791750f, 0.6385150558f, 0.5215872029f } } },
                { "SiC"   , { { 3.1723450205f, 2.5259677964f, 2.4793623897f }, {0.0000007284f, -0.0000006859f, 0.0000100150f } } },
                { "SnTe"  , { { 4.5251865890f, 1.9811525984f, 1.2816819226f }, { 0.0000000000f, 0.0000000000f, 0.0000000000f } } },
                { "Ta"    , { { 2.0625846607f, 2.3930915569f, 2.6280684948f }, { 2.4080467973f, 1.7413705864f, 1.9470377016f } } },
                { "Te-e"  , { { 7.5090397678f, 4.2964603080f, 2.3698732430f }, { 5.5842076830f, 4.9476231084f, 3.9975145063f } } },
                { "Te"    , { { 7.3908396088f, 4.4821028985f, 2.6370708478f }, { 3.2561412892f, 3.5273908133f, 3.2921683116f } } },
                { "ThF4"  , { { 1.8307187117f, 1.4422274283f, 1.3876488528f }, { 0.0000000000f, 0.0000000000f, 0.0000000000f } } },
                { "TiC"   , { { 3.7004673762f, 2.8374356509f, 2.5823030278f }, { 3.2656905818f, 2.3515586388f, 2.1727857800f } } },
                { "TiN"   , { { 1.6484691607f, 1.1504482522f, 1.3797795097f }, { 3.3684596226f, 1.9434888540f, 1.1020123347f } } },
                { "TiO2-e", { { 3.1065574823f, 2.5131551146f, 2.5823844157f }, {0.0000289537f, -0.0000251484f, 0.0001775555f } } },
                { "TiO2"  , { { 3.4566203131f, 2.8017076558f, 2.9051485020f }, {0.0001026662f, -0.0000897534f, 0.0006356902f } } },
                { "VC"    , { { 3.6575665991f, 2.7527298065f, 2.5326814570f }, { 3.0683516659f, 2.1986687713f, 1.9631816252f } } },
                { "VN"    , { { 2.8656011588f, 2.1191817791f, 1.9400767149f }, { 3.0323264950f, 2.0561075580f, 1.6162930914f } } },
                { "V"     , { { 4.2775126218f, 3.5131538236f, 2.7611257461f }, { 3.4911844504f, 2.8893580874f, 3.1116965117f } } },
                { "W"     , { { 4.3707029924f, 3.3002972445f, 2.9982666528f }, { 3.5006778591f, 2.6048652781f, 2.2731930614f } } }
            };

            auto iter = complexIorMap.find(name);

            if (iter == complexIorMap.end())
            {
                std::cerr << "Invalid complex IOR requested! Using gold ('Au') as default!" << '\n';
                return complexIorMap.at("Au");
            }

            return iter->second;
        }

        inline static RgbSpectrum conductor(float cosThetaI, const ComplexIOR& ior)
        {
            RgbSpectrum squareTerm = ior.eta * ior.eta + ior.k * ior.k;
            float cosTheta2 = cosThetaI * cosThetaI;
            RgbSpectrum etaCosTheta = 2.0f * ior.eta * cosThetaI;

            RgbSpectrum Rp = (squareTerm * cosTheta2 - etaCosTheta + 1) /
                             (squareTerm * cosTheta2 + etaCosTheta + 1);

            RgbSpectrum Rs = (squareTerm + cosTheta2 - etaCosTheta) /
                             (squareTerm + cosTheta2 + etaCosTheta);
                
            return (Rs + Rp) * 0.5f;
        }

        inline static RgbSpectrum conductorFull(float cosThetaI, const ComplexIOR& ior)
        {
            float cosTheta2 = cosThetaI * cosThetaI;
            float sinTheta2 = 1.0f - cosTheta2;
            glm::vec3 eta2(ior.eta.r * ior.eta.r, ior.eta.g * ior.eta.g, ior.eta.b * ior.eta.b);
            glm::vec3 k2(ior.k.r * ior.k.r, ior.k.g * ior.k.g, ior.k.b * ior.k.b);

            glm::vec3 t0 = eta2 - k2 - sinTheta2;
            glm::vec3 a2b2 = sqrt(t0 * t0 + 4.0f * eta2 * k2);
            glm::vec3 t1 = a2b2 + cosTheta2;
            glm::vec3 a = sqrt(0.5f * (a2b2 + t0));
            glm::vec3 t2 = 2.0f * a * cosThetaI;
            glm::vec3 Rs = (t1 - t2) / (t1 + t2);

            glm::vec3 t3 = cosTheta2 * a2b2 + sinTheta2 * sinTheta2;
            glm::vec3 t4 = t2 * sinTheta2;
            glm::vec3 Rp = Rs * (t3 - t4) / (t3 + t4);

            return (Rp + Rs) * 0.5f;
        }
    };
}