<script>
  let { title = 'XPA', children } = $props();
</script>

<div class="flex flex-col h-full">
  <!-- Titlebar -->
  <div class="flex items-center h-9 px-3 bg-panel-light border-b border-border select-none shrink-0">
    <span class="text-xs font-semibold tracking-wide text-text-dim uppercase">{title}</span>
  </div>

  <!-- Content area -->
  {@render children()}
</div>