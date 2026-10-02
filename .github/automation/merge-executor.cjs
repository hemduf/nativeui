'use strict';

const { createHash } = require('node:crypto');
const PREFIX = '/nativeui-merge\n';
const POLICY_PATHS = ['AGENTS.md', 'CODE_REVIEW.md', '.github/workflows/README.md'];
const GATES = ['scope', 'dependencies', 'acceptance', 'decisions', 'local', 'bookkeeping', 'integration'];
const POST_MERGE = ['main-smoke.yml', 'ci.yml', 'package-contract.yml', 'wasm.yml'];
const SHA = /^[0-9a-f]{40}$/;
const positive = value => Number.isSafeInteger(value) && value > 0;
const text = value => typeof value === 'string' && value.trim().length > 0;
function need(condition, message) { if (!condition) throw new Error(message); }
function digest(value) { return createHash('sha256').update(JSON.stringify(value)).digest('hex'); }

function parseRequest(body) {
  need(typeof body === 'string' && body.startsWith(PREFIX), 'Not a merge request');
  need(body.length <= 50000, 'Merge request is too large');
  const request = JSON.parse(body.slice(PREFIX.length));
  need(request.version === 1, 'Unsupported evidence version');
  need(['dry-run', 'merge', 'post-merge'].includes(request.mode), 'Explicit mode is required');
  need(SHA.test(request.head_sha) && SHA.test(request.base_sha), 'Full head/base SHA required');
  if (request.mode === 'post-merge') return request;
  const review = request.review;
  need(review && review.head_sha === request.head_sha && review.blocking === 0 && review.important === 0,
    'Clean review of the current head is required');
  need(text(review.summary) && Array.isArray(review.sources) && review.sources.length > 0,
    'Review summary and factual sources required');
  need(review.sources.some(source => ['review', 'comment'].includes(source.kind)), 'Recorded review evidence is required');
  need(request.inventory_complete === true, 'Applicable requirement inventory must be complete');
  for (const name of GATES) {
    const gate = request.gates?.[name];
    need(gate && ['pass', 'not_required'].includes(gate.result) && text(gate.reason), `Unresolved gate: ${name}`);
    need(Array.isArray(gate.sources) && gate.sources.length > 0, `Missing requirement/evidence sources: ${name}`);
  }
  need(Array.isArray(request.issues), 'Issue snapshots required, including an explicit empty list when not applicable');
  for (const issue of request.issues) need(positive(issue.number) && text(issue.updated_at), 'Invalid issue snapshot');
  for (const path of POLICY_PATHS) need(SHA.test(request.policies?.[path]), `Missing policy blob: ${path}`);
  need(Array.isArray(request.workflows) && request.workflows.length > 0, 'Required workflow inventory is empty');
  for (const workflow of request.workflows) {
    need(positive(workflow.id) && Array.isArray(workflow.jobs) && workflow.jobs.length > 0,
      'Each required workflow must enumerate its required jobs');
    need(workflow.jobs.every(text), 'Invalid required job name');
    for (const skip of workflow.skipped_jobs || []) {
      need(text(skip.name) && text(skip.reason) && Array.isArray(skip.sources) && skip.sources.length > 0,
        'Skipped jobs require an explicit, sourced non-required classification');
      need(!workflow.jobs.includes(skip.name), 'A required job cannot be exempted as skipped');
    }
  }
  need(Array.isArray(request.status_contexts) && request.status_contexts.every(text), 'Status context inventory required');
  need(Array.isArray(request.advisory_threads), 'Advisory thread inventory required');
  for (const thread of request.advisory_threads) {
    need(text(thread.id) && text(thread.updated_at) && text(thread.reason), 'Invalid advisory thread classification');
  }
  need(SHA.test(request.executable_sha || request.head_sha), 'Invalid executable SHA');
  return request;
}

function validatePull(pull, request, repository, mainSha, requireReady = false) {
  need(pull.state === 'open' && !pull.merged, 'PR is already closed or merged');
  need(pull.head?.repo?.full_name === repository, 'Fork PRs require the normal reviewed merge path');
  need(pull.base?.ref === 'main' && pull.base?.repo?.full_name === repository, 'Unexpected target repository or branch');
  need(pull.head.sha === request.head_sha, 'PR head changed');
  need(mainSha === request.base_sha && pull.base.sha === request.base_sha, 'Main changed; refresh the qualification snapshot');
  need(pull.mergeable === true, 'Mergeability is false or unknown');
  need(!['dirty', 'blocked', 'behind', 'unknown'].includes(pull.mergeable_state), 'GitHub reports a blocked merge state');
  if (requireReady) need(!pull.draft, 'PR is still Draft');
}

function validateJobs(workflow, run, jobs, allowedShas) {
  need(run.workflow_id === workflow.id && allowedShas.includes(run.head_sha), 'Workflow is not attached to an accepted exact SHA');
  need(run.status === 'completed' && run.conclusion === 'success', `Workflow ${workflow.id} is not successful`);
  need(jobs.length > 0, `Workflow ${workflow.id} has no jobs`);
  for (const name of workflow.jobs) need(jobs.some(job => job.name === name), `Required job missing: ${name}`);
  for (const job of jobs) {
    const exempt = job.conclusion === 'skipped' && (workflow.skipped_jobs || []).some(skip => skip.name === job.name);
    need(job.status === 'completed' && (job.conclusion === 'success' || exempt), `Non-successful job: ${job.name}`);
  }
}

function validateThreads(threads, request) {
  for (const thread of threads) {
    if (thread.isResolved) continue;
    const advisory = request.advisory_threads.find(item => item.id === thread.id);
    need(advisory && advisory.updated_at === thread.comments.nodes[0]?.updatedAt,
      `Unresolved or changed review thread: ${thread.id}`);
  }
}

async function execute({ github, context, core, body, pullNumber, actor, commandId }) {
  const request = parseRequest(body);
  const repo = context.repo;
  const repository = `${repo.owner}/${repo.repo}`;
  need(repository === 'hemduf/nativeui', 'Executor is restricted to its authorized repository');
  need(positive(pullNumber), 'Invalid PR number');
  const permission = (await github.rest.repos.getCollaboratorPermissionLevel({ ...repo, username: actor })).data.permission;
  need(['admin', 'maintain', 'write'].includes(permission), 'Request author lacks repository write permission');
  const getPull = async () => (await github.rest.pulls.get({ ...repo, pull_number: pullNumber })).data;
  const getMain = async () => (await github.rest.git.getRef({ ...repo, ref: 'heads/main' })).data.object.sha;
  const all = (method, args) => github.paginate(method, { ...repo, per_page: 100, ...args });
  const sourceCache = new Map();

  async function source(reference) {
    need(reference && positive(reference.id), 'Invalid evidence reference');
    const key = `${reference.kind}:${reference.id}`;
    if (sourceCache.has(key)) return sourceCache.get(key);
    let value;
    if (reference.kind === 'review') {
      value = (await github.rest.pulls.getReview({ ...repo, pull_number: pullNumber, review_id: reference.id })).data;
      need(!['DISMISSED', 'PENDING', 'CHANGES_REQUESTED'].includes(value.state), 'Review source is not clean/applicable');
    } else if (reference.kind === 'comment') {
      value = (await github.rest.issues.getComment({ ...repo, comment_id: reference.id })).data;
      need(reference.id !== commandId, 'A request cannot use itself as prior evidence');
    } else if (reference.kind === 'issue') {
      value = (await github.rest.issues.get({ ...repo, issue_number: reference.id })).data;
    } else if (reference.kind === 'run') {
      value = (await github.rest.actions.getWorkflowRun({ ...repo, run_id: reference.id })).data;
      need(value.status === 'completed' && value.conclusion === 'success', 'Evidence run is not successful');
    } else {
      throw new Error('Unsupported evidence reference kind');
    }
    const snapshot = { kind: reference.kind, id: reference.id, body: value.body, state: value.state,
      updated_at: value.updated_at, submitted_at: value.submitted_at, commit_id: value.commit_id,
      conclusion: value.conclusion, run_attempt: value.run_attempt, head_sha: value.head_sha };
    sourceCache.set(key, snapshot);
    return snapshot;
  }

  async function inspect(requireReady = false) {
    sourceCache.clear();
    const pull = await getPull();
    validatePull(pull, request, repository, await getMain(), requireReady);
    const policies = {};
    for (const path of [...POLICY_PATHS, 'CI_POLICY.md']) {
      let file;
      try { file = (await github.rest.repos.getContent({ ...repo, path, ref: request.base_sha })).data; }
      catch (error) { if (path === 'CI_POLICY.md' && error.status === 404) continue; throw error; }
      need(file.type === 'file' && file.sha === request.policies[path], `Policy changed or omitted: ${path}`);
      policies[path] = file.sha;
    }
    const issues = [];
    for (const expected of request.issues) {
      const issue = (await github.rest.issues.get({ ...repo, issue_number: expected.number })).data;
      need(issue.updated_at === expected.updated_at, `Issue requirements changed: #${expected.number}`);
      issues.push({ number: issue.number, body: issue.body, state: issue.state, state_reason: issue.state_reason });
    }
    const references = [...request.review.sources, ...GATES.flatMap(name => request.gates[name].sources),
      ...request.workflows.flatMap(workflow => (workflow.skipped_jobs || []).flatMap(skip => skip.sources))];
    const evidence = [];
    for (const reference of references) evidence.push(await source(reference));
    const executable = request.executable_sha || request.head_sha;
    if (executable !== request.head_sha) {
      const comparison = (await github.rest.repos.compareCommitsWithBasehead({ ...repo,
        basehead: `${executable}...${request.head_sha}` })).data;
      need(comparison.status === 'ahead' && comparison.merge_base_commit.sha === executable,
        'Executable qualification is not on an ancestor');
      need(comparison.files?.length === 1 && comparison.files[0].filename === 'ROADMAP.md'
        && comparison.files[0].status === 'modified', 'Executable qualification invalidated by non-bookkeeping changes');
    }
    const runs = await all(github.rest.actions.listWorkflowRunsForRepo, { head_sha: request.head_sha });
    const oldRuns = executable === request.head_sha ? [] :
      await all(github.rest.actions.listWorkflowRunsForRepo, { head_sha: executable });
    const validatedRuns = [];
    for (const workflow of request.workflows) {
      let candidates = runs.filter(run => run.workflow_id === workflow.id);
      if (!candidates.length) candidates = oldRuns.filter(run => run.workflow_id === workflow.id);
      need(candidates.length > 0, `Required workflow missing: ${workflow.id}`);
      const latestByEvent = new Map();
      for (const run of candidates.sort((a, b) => b.id - a.id)) {
        if (!latestByEvent.has(run.event)) latestByEvent.set(run.event, run);
      }
      for (const run of latestByEvent.values()) {
        const jobs = await all(github.rest.actions.listJobsForWorkflowRun, { run_id: run.id, filter: 'latest' });
        validateJobs(workflow, run, jobs, [request.head_sha, executable]);
        validatedRuns.push({ id: run.id, attempt: run.run_attempt, sha: run.head_sha,
          jobs: jobs.map(job => ({ id: job.id, name: job.name, conclusion: job.conclusion })) });
      }
    }
    const statuses = await all(github.rest.repos.listCommitStatusesForRef, { ref: request.head_sha });
    for (const name of request.status_contexts) {
      const latest = statuses.filter(status => status.context === name).sort((a, b) => b.id - a.id)[0];
      need(latest?.state === 'success', `Required commit status is not successful: ${name}`);
    }
    const reviews = await all(github.rest.pulls.listReviews, { pull_number: pullNumber });
    const votes = new Map();
    for (const review of reviews.sort((a, b) => a.id - b.id)) {
      if (['APPROVED', 'CHANGES_REQUESTED'].includes(review.state)) votes.set(review.user.login, review.state);
    }
    need(![...votes.values()].includes('CHANGES_REQUESTED'), 'An applicable review still requests changes');
    let cursor = null;
    const threads = [];
    do {
      const result = await github.graphql(`query($owner:String!,$repo:String!,$number:Int!,$cursor:String) {
        repository(owner:$owner,name:$repo) { pullRequest(number:$number) {
          reviewDecision reviewThreads(first:100,after:$cursor) {
            nodes { id isResolved comments(last:1) { nodes { updatedAt } } }
            pageInfo { hasNextPage endCursor }
          }
        } }
      }`, { ...repo, number: pullNumber, cursor });
      const pr = result.repository.pullRequest;
      need(!['CHANGES_REQUESTED', 'REVIEW_REQUIRED'].includes(pr.reviewDecision), 'GitHub review requirements are not satisfied');
      threads.push(...pr.reviewThreads.nodes);
      cursor = pr.reviewThreads.pageInfo.hasNextPage ? pr.reviewThreads.pageInfo.endCursor : null;
    } while (cursor);
    validateThreads(threads, request);
    return { pull, fingerprint: digest({ policies, issues, evidence, validatedRuns, statuses,
      reviews: reviews.map(review => ({ id: review.id, state: review.state, body: review.body })), threads }) };
  }

  async function confirmAndDispatch(pull) {
    need(pull.base?.ref === 'main' && pull.base?.repo?.full_name === repository,
      'Post-merge recovery is restricted to merges into this repository main');
    need(pull.merged && pull.head.sha === request.head_sha && SHA.test(pull.merge_commit_sha), 'Merge not confirmed');
    const main = await getMain();
    const comparison = (await github.rest.repos.compareCommitsWithBasehead({ ...repo,
      basehead: `${pull.merge_commit_sha}...${main}` })).data;
    need(['identical', 'ahead'].includes(comparison.status), 'Merged commit is not present on main');
    core.setOutput('merged_sha', pull.merge_commit_sha);
    const errors = [];
    const runs = await all(github.rest.actions.listWorkflowRunsForRepo, { head_sha: pull.merge_commit_sha });
    for (const workflow of POST_MERGE) {
      if (runs.some(run => run.path?.split('@')[0] === `.github/workflows/${workflow}`)) continue;
      try {
        need(await getMain() === pull.merge_commit_sha, 'Main advanced; qualify the new integrated SHA instead of dispatching an ambiguous ref');
        // Existing workflows already accept workflow_dispatch and checkout their event SHA.
        // Do not pass unsupported inputs or modify their tests, platforms or permissions.
        await github.rest.actions.createWorkflowDispatch({ ...repo, workflow_id: workflow, ref: 'main' });
        need(await getMain() === pull.merge_commit_sha, 'Main moved during dispatch; verify the actual run SHA before qualification');
      } catch (error) { errors.push(`${workflow}: ${error.status || 'unknown'} ${error.message}`); }
    }
    if (errors.length) {
      core.setOutput('post_merge_pending', 'true');
      throw new Error(`Merge confirmed at ${pull.merge_commit_sha}; post-merge dispatch incomplete: ${errors.join('; ')}`);
    }
    core.setOutput('post_merge_pending', 'false');
    return { state: 'merged', sha: pull.merge_commit_sha, post_merge: 'dispatch requested; exact run SHA and results still require verification' };
  }

  const initial = await getPull();
  if (request.mode === 'post-merge') return confirmAndDispatch(initial);
  if (initial.merged || initial.state !== 'open') return { state: 'already-closed', mutated: false };
  const qualified = await inspect();
  if (request.mode === 'dry-run') return { state: 'qualified-dry-run', head: request.head_sha, mutated: false };
  if (qualified.pull.draft) {
    await github.graphql('mutation($id:ID!) { markPullRequestReadyForReview(input:{pullRequestId:$id}) { pullRequest { id } } }',
      { id: qualified.pull.node_id });
  }
  const final = await inspect(true);
  need(final.fingerprint === qualified.fingerprint, 'Evidence changed during closeout; no merge attempted');
  const result = (await github.rest.pulls.merge({ ...repo, pull_number: pullNumber,
    sha: request.head_sha, merge_method: 'merge' })).data;
  need(result.merged === true, `GitHub did not merge the PR: ${result.message || 'unknown reason'}`);
  core.setOutput('merged_sha', result.sha);
  return confirmAndDispatch(await getPull());
}

module.exports = { PREFIX, POLICY_PATHS, GATES, POST_MERGE, parseRequest, validatePull, validateJobs, validateThreads, execute };
