export const VEHICLES = {
  "car": {
    "entity": 7000,
    "name": "Birchside hatchback",
    "kind": "car",
    "position": [
      12,
      14.934437565398273,
      43
    ],
    "mass": 950,
    "half": [
      0.78,
      0.3,
      1.9
    ],
    "height": 0.9,
    "radius": 0.31,
    "track": 0.75,
    "front": -1.3,
    "rear": 1.25,
    "slots": 16,
    "capacity": 160,
    "tank": 40,
    "fuel": 28,
    "burn": 0.055,
    "color": "#457b72",
    "wheels": [
      "7002",
      "7003",
      "7004",
      "7005"
    ],
    "visual": "7001"
  },
  "truck": {
    "entity": 7100,
    "name": "Homestead pickup",
    "kind": "truck",
    "position": [
      -8,
      15.141300398881427,
      43
    ],
    "mass": 1400,
    "half": [
      0.9,
      0.34,
      2.65
    ],
    "height": 1.12,
    "radius": 0.4,
    "track": 0.87,
    "front": -1.8,
    "rear": 1.65,
    "slots": 30,
    "capacity": 650,
    "tank": 70,
    "fuel": 45,
    "burn": 0.09,
    "color": "#845046",
    "wheels": [
      "7102",
      "7103",
      "7104",
      "7105"
    ],
    "visual": "7101"
  },
  "motorcycle": {
    "entity": 7200,
    "name": "Trail motorcycle",
    "kind": "motorcycle",
    "position": [
      22,
      14.909358651856538,
      43
    ],
    "mass": 180,
    "half": [
      0.22,
      0.32,
      0.95
    ],
    "height": 0.85,
    "radius": 0.33,
    "track": 0,
    "front": -0.85,
    "rear": 0.85,
    "slots": 8,
    "capacity": 40,
    "tank": 14,
    "fuel": 10,
    "burn": 0.025,
    "color": "#b6a65d",
    "wheels": [
      "7202",
      "7203"
    ],
    "visual": "7201"
  },
  "boat": {
    "entity": 7300,
    "name": "Spring skiff",
    "kind": "boat",
    "position": [
      90,
      10.05,
      -98
    ],
    "mass": 650,
    "half": [
      0.8,
      0.3,
      2.2
    ],
    "height": 10.05,
    "slots": 16,
    "capacity": 220,
    "tank": 30,
    "fuel": 22,
    "burn": 0.065,
    "color": "#d0cec0",
    "wheels": [],
    "visual": "7301"
  },
  "helicopter": {
    "entity": 7400,
    "name": "Coast rescue helicopter",
    "kind": "helicopter",
    "position": [
      -40,
      14.860985225281683,
      42
    ],
    "mass": 800,
    "half": [
      0.8,
      0.65,
      1.65
    ],
    "height": 1.25,
    "slots": 20,
    "capacity": 280,
    "tank": 120,
    "fuel": 90,
    "burn": 0.23,
    "color": "#c1a450",
    "wheels": [],
    "visual": "7401",
    "rotor": "7412",
    "flight": {
      "tilt": 0.48,
      "linearDrag": 0.1,
      "climbSpeed": 5,
      "descentSpeed": 3,
      "hoverInput": 0.523,
      "cruiseSpeed": 45,
      "horizontalResponse": 0.6
    }
  },
  "plane": {
    "entity": 7500,
    "name": "Island bush plane",
    "kind": "plane",
    "position": [
      0,
      16.121577577420858,
      185
    ],
    "mass": 650,
    "half": [
      0.48,
      0.38,
      2.65
    ],
    "height": 1.0,
    "radius": 0.3,
    "track": 1.25,
    "front": -1.9,
    "rear": 1.0,
    "slots": 16,
    "capacity": 180,
    "tank": 90,
    "fuel": 65,
    "burn": 0.15,
    "color": "#b65546",
    "wheels": [
      "7502",
      "7503",
      "7504"
    ],
    "visual": "7501",
    "rotor": "7512"
  },
  "depot": {
    "entity": 7800,
    "name": "Homestead fuel & parts locker",
    "kind": "depot",
    "position": [
      3,
      14.67879322876322,
      43
    ],
    "mass": 100,
    "half": [
      0.65,
      0.65,
      0.5
    ],
    "height": 0.65,
    "slots": 18,
    "capacity": 200,
    "tank": 0,
    "fuel": 0,
    "burn": 0,
    "stock": {
      "fuel_can": 12,
      "repair_kit": 8
    },
    "wheels": [],
    "visual": "7801"
  },
  "garage_truck": {
    "entity": 7600,
    "name": "Birchside service pickup",
    "kind": "truck",
    "position": [
      35,
      15.25555894244264,
      49
    ],
    "mass": 1400,
    "half": [
      0.9,
      0.34,
      2.65
    ],
    "height": 1.12,
    "radius": 0.4,
    "track": 0.87,
    "front": -1.8,
    "rear": 1.65,
    "slots": 30,
    "capacity": 650,
    "tank": 70,
    "fuel": 45,
    "burn": 0.09,
    "color": "#845046",
    "region": "transport_garage",
    "stock": {
      "wood": 4,
      "fuel_can": 1
    },
    "wheels": [
      "7602",
      "7603",
      "7604",
      "7605"
    ],
    "visual": "7601"
  },
  "region_harbour_boat": {
    "entity": 36000,
    "name": "Tidemark Harbour ocean skiff",
    "kind": "boat",
    "position": [
      -3895.8,
      3345
    ],
    "mass": 650,
    "half": [
      0.8,
      0.3,
      2.2
    ],
    "height": 0.05,
    "slots": 16,
    "capacity": 220,
    "tank": 30,
    "fuel": 22,
    "burn": 0.065,
    "color": "#d0cec0",
    "region": "harbour",
    "stock": {
      "fuel_can": 2,
      "rations": 2
    }
  },
  "region_islet_boat": {
    "entity": 76000,
    "name": "Lantern Islet ocean skiff",
    "kind": "boat",
    "position": [
      2404.2,
      3145
    ],
    "mass": 650,
    "half": [
      0.8,
      0.3,
      2.2
    ],
    "height": 0.05,
    "slots": 16,
    "capacity": 220,
    "tank": 30,
    "fuel": 22,
    "burn": 0.065,
    "color": "#d0cec0",
    "region": "islet",
    "stock": {
      "fuel_can": 2,
      "rations": 2
    }
  }
};
