// nodes.js — AuraAbilityGraph Node type definitions, categories, property schemas
// IMPORTANT: type strings MUST match CreateNodeByClassName() in AbilityNodeRegistry.cpp exactly!

const NodeCategory = { COMPOSITE:'composite', ACTION:'action' };

const CAT_COLOR = {
  composite: { bg:'#1a2d45', header:'#2980b9', port:'#3498db' },
  action:    { bg:'#1a3328', header:'#27ae60', port:'#2ecc71' },
};

// ── Property schemas ─────────────────────────────────────
// Keys match Prop.Name strings in the C++ node classes exactly
const NODE_PROPS = {
  // ── Composites ──────────────────────────────────────────
  Sequence: [],

  // ── Actions ─────────────────────────────────────────────
  PlayMontage: [],
  WaitForMontageEvent: [
    { key:'EventTag', label:'Event Tag', type:'string', default:'' }
  ],
  WaitForTargetData: [],
  SpawnProjectile: [
    { key:'SocketTag', label:'Socket Tag', type:'string', default:'' },
    { key:'ProjectileClass', label:'Projectile Class', type:'string', default:'' }
  ],
  SpawnProjectiles: [
    { key:'SocketTag', label:'Socket Tag', type:'string', default:'' },
    { key:'ProjectileClass', label:'Projectile Class', type:'string', default:'' },
    { key:'Count', label:'Count', type:'string', default:'5' },
    { key:'Spread', label:'Spread', type:'string', default:'90' },
    { key:'bHoming', label:'Homing', type:'bool', default:false },
    { key:'HomingAccelerationMin', label:'Homing Accel Min', type:'string', default:'0' },
    { key:'HomingAccelerationMax', label:'Homing Accel Max', type:'string', default:'0' },
    { key:'TargetFromContext', label:'Target From Context', type:'string', default:'' }
  ],
  HitscanTrace: [
    { key:'SocketTag', label:'Socket Tag', type:'string', default:'' },
    { key:'TraceRange', label:'Trace Range', type:'string', default:'10000' },
    { key:'ScatterRadius', label:'Scatter Radius', type:'string', default:'50' }
  ],
  FaceTarget: [
    { key:'bSetControllerRotation', label:'Set Controller Rotation', type:'bool', default:true },
    { key:'bSetActorRotation', label:'Set Actor Rotation', type:'bool', default:true },
    { key:'bYawOnly', label:'Yaw Only', type:'bool', default:false }
  ],
  Wait: [
    { key:'Seconds', label:'Seconds', type:'string', default:'0.5' }
  ],
  ApplyDamage: [
    { key:'TargetFromContext', label:'Target From Context', type:'string', default:'' }
  ],
  CauseDamage: [
    { key:'TargetFromContext', label:'Target From Context', type:'string', default:'' }
  ],
  MulticastGunFX: [
    { key:'MuzzleSocketTag', label:'Muzzle Socket Tag', type:'string', default:'' },
    { key:'MuzzleEffect', label:'Muzzle Effect', type:'string', default:'' },
    { key:'FireSound', label:'Fire Sound', type:'string', default:'' }
  ],
};

// ── Master node list ─────────────────────────────────────
const NODE_TYPES = [
  { type:'Sequence',              category:NodeCategory.COMPOSITE, label:'Sequence',              tooltip:'Runs children left-to-right. Stops on failure.' },
  { type:'PlayMontage',           category:NodeCategory.ACTION,    label:'Play Montage',          tooltip:'Plays the ability montage.' },
  { type:'WaitForMontageEvent',   category:NodeCategory.ACTION,    label:'Wait For Montage Event',tooltip:'Waits for a gameplay event from the montage.' },
  { type:'WaitForTargetData',     category:NodeCategory.ACTION,    label:'Wait For Target Data',  tooltip:'Waits for target data from client.' },
  { type:'SpawnProjectile',       category:NodeCategory.ACTION,    label:'Spawn Projectile',      tooltip:'Spawns a single projectile.' },
  { type:'SpawnProjectiles',      category:NodeCategory.ACTION,    label:'Spawn Projectiles',     tooltip:'Spawns multiple projectiles.' },
  { type:'HitscanTrace',          category:NodeCategory.ACTION,    label:'Hitscan Trace',         tooltip:'Line trace from socket.' },
  { type:'ApplyDamage',           category:NodeCategory.ACTION,    label:'Apply Damage',          tooltip:'Applies damage effect to target.' },
  { type:'CauseDamage',           category:NodeCategory.ACTION,    label:'Cause Damage',          tooltip:'Causes damage with impulse/knockback.' },
  { type:'MulticastGunFX',        category:NodeCategory.ACTION,    label:'Multicast Gun FX',      tooltip:'Plays muzzle FX and sound.' },
  { type:'FaceTarget',            category:NodeCategory.ACTION,    label:'Face Target',          tooltip:'Turn the avatar toward the trigger/cursor direction.' },
  { type:'Wait',                  category:NodeCategory.ACTION,    label:'Wait',                 tooltip:'Waits for a specified number of seconds before continuing.' },
];

const NODE_TYPE_MAP = Object.fromEntries(NODE_TYPES.map(n => [n.type, n]));

function getNodeProps(type)  { return NODE_PROPS[type] || []; }
function getCatColor(cat)    { return CAT_COLOR[cat] || CAT_COLOR.composite; }
function isComposite(type)   { return NODE_TYPE_MAP[type]?.category === NodeCategory.COMPOSITE; }
function isAction(type)      { return NODE_TYPE_MAP[type]?.category === NodeCategory.ACTION; }
function canHaveChildren(type) { return isComposite(type); }
function maxChildren(type) {
  if (!canHaveChildren(type)) return 0;
  return Infinity;
}
function getDisplayLabel(type) {
  return NODE_TYPE_MAP[type]?.label || type;
}
