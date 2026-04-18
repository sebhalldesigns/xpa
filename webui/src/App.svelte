<script>
  import Workbench from './lib/Workbench.svelte';
  import Panel from './lib/Panel.svelte';
</script>

<Workbench title="XPA Test">
  {#snippet children()}
    <div class="flex flex-1 overflow-hidden">
      <Panel title="Explorer" width="240px">
        {#snippet children()}
          <div class="p-3 text-sm text-text-dim">
            <p>Tree view would go here</p>
          </div>
        {/snippet}
      </Panel>

      <div class="flex flex-col flex-1">
        <Panel title="Main View" flex>
          {#snippet children()}
            <div class="flex items-center justify-center h-full text-text-dim">
              <p>GPU viewport or document content</p>
            </div>
          {/snippet}
        </Panel>

        <Panel title="Output" height="200px">
          {#snippet children()}
            <div class="p-3 text-sm font-mono text-text-dim">
              <p>[info] Ready</p>
              <p>[info] Connected to CAN0</p>
            </div>
          {/snippet}
        </Panel>
      </div>

      <Panel title="Properties" width="280px">
        {#snippet children()}
          <div class="p-3 text-sm space-y-2">
            <div class="flex justify-between">
              <span class="text-text-dim">Signal</span>
              <span>EngineRPM</span>
            </div>
            <div class="flex justify-between">
              <span class="text-text-dim">Value</span>
              <span>3421.5</span>
            </div>
            <div class="flex justify-between">
              <span class="text-text-dim">Unit</span>
              <span>rpm</span>
            </div>
          </div>
        {/snippet}
      </Panel>
    </div>
  {/snippet}
</Workbench>