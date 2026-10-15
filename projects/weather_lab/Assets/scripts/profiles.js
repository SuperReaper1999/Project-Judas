// Game-side authoring recipes. These are values in ordinary WeatherProfiles,
// not special native weather modes. Change them and reload without recompiling.
const calm={cloudCoverage:.12,cloudDensity:.65,rain:0,snow:0,fog:0,wind:{x:3,y:0,z:1},gust:1,lightningRate:0,tornado:0,tornadoRadius:17,tornadoHeight:320,tornadoSpeed:2};
export const names=['sunshine','gathering clouds','rain','thunderstorm','tornado','snowfall','blizzard','fog'];
export const recipes=[
 {...calm},
 {...calm,cloudCoverage:.76,cloudDensity:1.15,gust:4},
 {...calm,cloudCoverage:.94,cloudDensity:1.45,rain:.8,fog:.15,wind:{x:8,y:0,z:3},gust:5},
 {...calm,cloudCoverage:1,cloudDensity:1.85,rain:1,fog:.28,wind:{x:14,y:0,z:6},gust:11,lightningRate:28},
 {...calm,cloudCoverage:1,cloudDensity:1.9,rain:.65,fog:.25,wind:{x:13,y:0,z:5},gust:12,lightningRate:18,tornado:1},
 {...calm,cloudCoverage:.8,cloudDensity:1.15,snow:.8,fog:.2,wind:{x:4,y:0,z:2},gust:4},
 {...calm,cloudCoverage:1,cloudDensity:1.6,snow:1,fog:.72,wind:{x:22,y:0,z:8},gust:13},
 {...calm,cloudCoverage:.4,fog:.82,wind:{x:1,y:0,z:0},gust:.5}
];
export const tourDurations=[8,10,10,12,18,10,12,8];
