<script>
  let { tabs = [], active = 0, onselect } = $props();
</script>

<div class="flex h-8 bg-panel-light border-b border-border overflow-x-auto">
  {#each tabs as tab, i}
    <button
      class="px-4 text-xs border-r border-border transition-colors
             {i === active ? 'bg-panel text-text border-b-2 border-b-accent' : 'text-text-dim hover:text-text hover:bg-panel'}"
      onclick={() => onselect?.(i)}
    >
      {tab}
    </button>
  {/each}
</div>