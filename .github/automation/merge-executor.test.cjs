'use strict';

const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const engine = require('./merge-executor.cjs');
const HEAD = 'a'.repeat(40);
const BASE = 'b'.repeat(40);
const BLOB = 'c'.repeat(40);
const MERGE = 'd'.repeat(40);
const stamp = '2026-01-01T00:00:00Z';

function evidence() {
  return {
    version: 1, mode: 'dry-run', head_sha: HEAD, base_sha: BASE,
    review: { head_sha: HEAD, blocking: 0, important: 0, summary: 'Current head reviewed; no remaining findings.',
      sources: [{ kind: 'review', id: 7 }] },
    inventory_complete: true,
    gates: Object.fromEntries(engine.GATES.map(name => [name,
      { result: 'pass', reason: 'Applicable requirement checked against recorded evidence.', sources: [{ kind: 'issue', id: 9 }] }])),
    issues: [{ number: 9, updated_at: stamp }],
    policies: Object.fromEntries(engine.POLICY_PATHS.map(name => [name, BLOB])),
    workflows: [{ id: 11, jobs: ['unit', 'platform'] }],
    status_contexts: [], advisory_threads: []
  };
}
function encode(value) { return engine.PREFIX + JSON.stringify(value); }
function pull() {
  return { number: 8, node_id: 'pull-node', state: 'open', merged: false, draft: true,
    mergeable: true, mergeable_state: 'clean', head: { sha: HEAD, repo: { full_name: 'hemduf/nativeui' } },
    base: { ref: 'main', sha: BASE, repo: { full_name: 'hemduf/nativeui' } } };
}
function fixture() {
  const state = { pull: pull(), main: BASE, writes: [], permission: 'write', issueDate: stamp,
    reviewDecision: null, threads: [], reviews: [], sourceBody: 'Recorded evidence',
    runs: [{ id: 20, workflow_id: 11, head_sha: HEAD, event: 'pull_request', status: 'completed', conclusion: 'success', run_attempt: 1 }],
    jobs: ['unit', 'platform'].map((name, i) => ({ id: i + 1, name, status: 'completed', conclusion: 'success' })),
    statuses: [], beforeReady: null, mergeError: null, dispatchError: null, dispatchHook: null };
  const response = data => ({ data });
  const github = { rest: {
    repos: {
      getCollaboratorPermissionLevel: async () => response({ permission: state.permission }),
      getContent: async ({ path: name }) => {
        if (name === 'CI_POLICY.md') throw Object.assign(new Error('Not found'), { status: 404 });
        return response({ type: 'file', sha: BLOB });
      },
      compareCommitsWithBasehead: async ({ basehead }) => response(basehead.startsWith(`${MERGE}...`)
        ? { status: 'identical' } : { status: 'ahead', merge_base_commit: { sha: 'e'.repeat(40) },
          files: [{ filename: 'ROADMAP.md', status: 'modified' }] }),
      listCommitStatusesForRef: async () => response(state.statuses)
    },
    pulls: {
      get: async () => response(structuredClone(state.pull)),
      getReview: async () => response({ id: 7, state: 'COMMENTED', body: state.sourceBody, commit_id: HEAD }),
      listReviews: async () => response(state.reviews),
      merge: async args => {
        state.writes.push(['merge', args]);
        if (state.mergeError) throw state.mergeError;
        state.pull.merged = true; state.pull.state = 'closed'; state.pull.merge_commit_sha = MERGE;
        state.main = MERGE;
        return response({ merged: true, sha: MERGE });
      }
    },
    issues: {
      get: async ({ issue_number }) => response({ number: issue_number, body: 'Acceptance and execution evidence.',
        state: 'open', updated_at: state.issueDate }),
      getComment: async () => response({ body: state.sourceBody, updated_at: stamp })
    },
    git: { getRef: async () => response({ object: { sha: state.main } }) },
    actions: {
      listWorkflowRunsForRepo: async () => response(state.runs),
      listJobsForWorkflowRun: async () => response(state.jobs),
      getWorkflowRun: async () => response(state.runs[0]),
      createWorkflowDispatch: async args => {
        state.writes.push(['dispatch', args]);
        if (state.dispatchError) throw state.dispatchError;
        state.dispatchHook?.();
        return { status: 204 };
      }
    }
  },
  paginate: async (method, args) => (await method(args)).data,
  graphql: async query => {
    if (query.startsWith('mutation')) {
      state.writes.push(['ready']); state.pull.draft = false;
      state.beforeReady?.();
      return {};
    }
    return { repository: { pullRequest: { reviewDecision: state.reviewDecision,
      reviewThreads: { nodes: state.threads, pageInfo: { hasNextPage: false, endCursor: null } } } } };
  } };
  const outputs = {};
  const options = request => ({ github, context: { repo: { owner: 'hemduf', repo: 'nativeui' } },
    core: { setOutput: (key, value) => { outputs[key] = value; } },
    body: encode(request), pullNumber: 8, actor: 'maintainer', commandId: 99 });
  return { state, github, outputs, options };
}

test('an explicit dry-run request is accepted', () => assert.equal(engine.parseRequest(encode(evidence())).mode, 'dry-run'));
for (const [name, mutate] of [
  ['missing explicit mode', x => { delete x.mode; }],
  ['abbreviated SHA', x => { x.head_sha = 'abcdef'; }],
  ['unclean review', x => { x.review.important = 1; }],
  ['review on a stale head', x => { x.review.head_sha = BASE; }],
  ['incomplete inventory', x => { x.inventory_complete = false; }],
  ['pending local gate', x => { x.gates.local.result = 'pending'; }],
  ['unsupported local exemption', x => { x.gates.local.sources = []; }],
  ['missing workflow inventory', x => { x.workflows = []; }],
  ['missing job inventory', x => { x.workflows[0].jobs = []; }],
  ['missing policy version', x => { delete x.policies['CODE_REVIEW.md']; }],
  ['required job exemption', x => { x.workflows[0].skipped_jobs = [{ name: 'unit', reason: 'Not allowed', sources: [{ kind: 'issue', id: 9 }] }]; }]
]) test(`rejects ${name}`, () => { const x = evidence(); mutate(x); assert.throws(() => engine.parseRequest(encode(x))); });

test('dry-run makes zero mutations including Ready', async () => {
  const f = fixture(); const result = await engine.execute(f.options(evidence()));
  assert.equal(result.state, 'qualified-dry-run'); assert.deepEqual(f.state.writes, []);
});
test('a qualified request is readied then merged with its exact SHA and dispatches existing integration workflows', async () => {
  const f = fixture(); const x = evidence(); x.mode = 'merge';
  const result = await engine.execute(f.options(x));
  assert.equal(result.state, 'merged');
  assert.deepEqual(f.state.writes.map(item => item[0]), ['ready', 'merge', 'dispatch', 'dispatch', 'dispatch', 'dispatch']);
  assert.equal(f.state.writes[1][1].sha, HEAD);
  assert.equal(f.state.writes[1][1].merge_method, 'merge');
  for (const [, args] of f.state.writes.slice(2)) {
    assert.equal(args.ref, 'main'); assert.equal(args.inputs, undefined);
  }
  assert.equal(f.outputs.merged_sha, MERGE);
});
for (const [name, mutate] of [
  ['read-only request author', s => { s.permission = 'read'; }],
  ['changed head', s => { s.pull.head.sha = BASE; }],
  ['changed main', s => { s.main = HEAD; }],
  ['unknown mergeability', s => { s.pull.mergeable = null; }],
  ['conflicting branch', s => { s.pull.mergeable_state = 'dirty'; }],
  ['fork source', s => { s.pull.head.repo.full_name = 'other/nativeui'; }],
  ['changed issue requirements', s => { s.issueDate = '2026-01-02T00:00:00Z'; }],
  ['missing workflow', s => { s.runs = []; }],
  ['pending workflow', s => { s.runs[0].status = 'in_progress'; }],
  ['failed workflow', s => { s.runs[0].conclusion = 'failure'; }],
  ['wrong workflow SHA', s => { s.runs[0].head_sha = BASE; }],
  ['missing job', s => { s.jobs.pop(); }],
  ['skipped job', s => { s.jobs[0].conclusion = 'skipped'; }],
  ['required review', s => { s.reviewDecision = 'REVIEW_REQUIRED'; }],
  ['changes requested', s => { s.reviews = [{ id: 4, state: 'CHANGES_REQUESTED', user: { login: 'reviewer' } }]; }],
  ['unresolved thread', s => { s.threads = [{ id: 'thread', isResolved: false, comments: { nodes: [{ updatedAt: stamp }] } }]; }]
]) test(`${name} prevents every mutation`, async () => {
  const f = fixture(); mutate(f.state); const x = evidence(); x.mode = 'merge';
  await assert.rejects(engine.execute(f.options(x))); assert.deepEqual(f.state.writes, []);
});
test('a sourced optional skipped job does not become an invented gate', async () => {
  const f = fixture(); const x = evidence();
  f.state.jobs.push({ id: 3, name: 'optional', status: 'completed', conclusion: 'skipped' });
  x.workflows[0].skipped_jobs = [{ name: 'optional', reason: 'Documented conditional job does not apply.', sources: [{ kind: 'issue', id: 9 }] }];
  assert.equal((await engine.execute(f.options(x))).state, 'qualified-dry-run');
});
test('an older failure replaced by current success is not a gate', async () => {
  const f = fixture(); f.state.runs.push({ ...f.state.runs[0], id: 19, conclusion: 'failure' });
  assert.equal((await engine.execute(f.options(evidence()))).state, 'qualified-dry-run');
});
test('a newer pending run cannot be hidden by an old green run', async () => {
  const f = fixture(); f.state.runs.push({ ...f.state.runs[0], id: 21, status: 'queued', conclusion: null });
  await assert.rejects(engine.execute(f.options(evidence())), /not successful/);
});
test('a failing push run is not hidden by a successful concurrent PR run', async () => {
  const f = fixture(); f.state.runs.push({ ...f.state.runs[0], id: 19, event: 'push', conclusion: 'failure' });
  await assert.rejects(engine.execute(f.options(evidence())), /not successful/);
});
test('classified unchanged advisory threads do not require resolution', async () => {
  const f = fixture(); const x = evidence();
  f.state.threads = [{ id: 'thread', isResolved: false, comments: { nodes: [{ updatedAt: stamp }] } }];
  x.advisory_threads = [{ id: 'thread', updated_at: stamp, reason: 'Editorial suggestion; no acceptance impact.' }];
  assert.equal((await engine.execute(f.options(x))).state, 'qualified-dry-run');
});
test('a changed thread invalidates the advisory classification', async () => {
  const f = fixture(); const x = evidence();
  f.state.threads = [{ id: 'thread', isResolved: false, comments: { nodes: [{ updatedAt: stamp }] } }];
  x.advisory_threads = [{ id: 'thread', updated_at: 'older', reason: 'Editorial.' }];
  await assert.rejects(engine.execute(f.options(x)), /changed review thread/);
});
test('head movement after Ready prevents merge', async () => {
  const f = fixture(); const x = evidence(); x.mode = 'merge';
  f.state.beforeReady = () => { f.state.pull.head.sha = BASE; };
  await assert.rejects(engine.execute(f.options(x)), /head changed/);
  assert.deepEqual(f.state.writes.map(item => item[0]), ['ready']);
});
test('edited evidence after Ready prevents merge', async () => {
  const f = fixture(); const x = evidence(); x.mode = 'merge';
  f.state.beforeReady = () => { f.state.sourceBody = 'Review changed'; };
  await assert.rejects(engine.execute(f.options(x)), /Evidence changed/);
  assert.deepEqual(f.state.writes.map(item => item[0]), ['ready']);
});
test('merge rejection is never retried or replaced with a ref update', async () => {
  const f = fixture(); const x = evidence(); x.mode = 'merge';
  f.state.mergeError = Object.assign(new Error('Protected branch'), { status: 403 });
  await assert.rejects(engine.execute(f.options(x)), /Protected branch/);
  assert.equal(f.state.writes.filter(item => item[0] === 'merge').length, 1);
  assert.equal(f.state.writes.filter(item => item[0] === 'dispatch').length, 0);
});
test('post-merge dispatch failure preserves the confirmed merge result', async () => {
  const f = fixture(); const x = evidence(); x.mode = 'merge';
  f.state.dispatchError = Object.assign(new Error('Actions permission denied'), { status: 403 });
  await assert.rejects(engine.execute(f.options(x)), /Merge confirmed.*dispatch incomplete/);
  assert.equal(f.outputs.merged_sha, MERGE); assert.equal(f.outputs.post_merge_pending, 'true');
});
test('main moving during dispatch is reported without dispatching further ambiguous refs', async () => {
  const f = fixture(); const x = evidence(); x.mode = 'merge';
  f.state.dispatchHook = () => { f.state.main = HEAD; };
  await assert.rejects(engine.execute(f.options(x)), /Main moved during dispatch/);
  assert.equal(f.state.writes.filter(item => item[0] === 'dispatch').length, 1);
  assert.equal(f.outputs.merged_sha, MERGE);
});
test('post-merge recovery does not attempt Ready or merge again', async () => {
  const f = fixture(); const x = evidence(); x.mode = 'post-merge';
  f.state.pull.merged = true; f.state.pull.state = 'closed'; f.state.pull.merge_commit_sha = MERGE; f.state.main = MERGE;
  await engine.execute(f.options(x));
  assert.equal(f.state.writes.filter(item => item[0] === 'merge' || item[0] === 'ready').length, 0);
});
test('existing post-merge runs are not dispatched again', async () => {
  const f = fixture(); const x = evidence(); x.mode = 'post-merge';
  f.state.pull.merged = true; f.state.pull.state = 'closed'; f.state.pull.merge_commit_sha = MERGE; f.state.main = MERGE;
  f.state.runs = engine.POST_MERGE.map(file => ({ path: `.github/workflows/${file}`, head_sha: MERGE, status: 'queued' }));
  await engine.execute(f.options(x)); assert.deepEqual(f.state.writes, []);
});
test('post-merge recovery refuses merges into another branch', async () => {
  const f = fixture(); const x = evidence(); x.mode = 'post-merge';
  f.state.pull.merged = true; f.state.pull.merge_commit_sha = MERGE; f.state.pull.base.ref = 'other';
  await assert.rejects(engine.execute(f.options(x)), /restricted to merges/);
  assert.deepEqual(f.state.writes, []);
});
test('closed PRs are not processed again', async () => {
  const f = fixture(); f.state.pull.state = 'closed';
  assert.equal((await engine.execute(f.options(evidence()))).state, 'already-closed');
  assert.deepEqual(f.state.writes, []);
});
test('privileged workflow never checks out or runs PR code', () => {
  const content = fs.readFileSync(path.join(__dirname, '../workflows/qualified-merge.yml'), 'utf8');
  assert.doesNotMatch(content, /actions\/checkout|pull_request_target|secrets\./);
  assert.match(content, /workflow_sha/);
  assert.match(content, /Module/);
  assert.match(content, /cancel-in-progress: false/);
});
